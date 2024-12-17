#include "decode.h"
#include <QImage>
#include <QPixmap>
#define MAX_AUDIO_FRAME_SIZE 192000
QString av_get_pixelformat_name(AVPixelFormat format);

Decode::Decode(const int *_type, QObject *parent) : m_type(_type), QObject(parent), formatContext(nullptr), videoCodecContext(nullptr), audioCodecContext(nullptr), swrContext(nullptr), convertedAudioBuffer(nullptr), audioStreamIndex(-1), videoStreamIndex(-1), mediaType(UNKNOWN), time_base_q2d(0), curPts(0)
{
    avformat_network_init(); // Initialize FFmpeg network components

    // 遍历出设备支持的硬件类型
    enum AVHWDeviceType print_type = AV_HWDEVICE_TYPE_NONE;
    while ((print_type = av_hwdevice_iterate_types(print_type)) != AV_HWDEVICE_TYPE_NONE)
    {
        qDebug() << "suport devices: " << av_hwdevice_get_type_name(print_type);
        devices.append(print_type);
    }
}

Decode::~Decode()
{
    clean();
}

bool Decode::resume()
{
    curPts = 0;
    av_seek_frame(formatContext, audioStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    return true;
}

void Decode::setVideoPath(const QString &filePath)
{
    clean();
    isIniting = true;
    if (NO_ERROR == initFFmpeg(filePath))
    {
        isIniting = false;
        isInitSuccess = true;
        qDebug() << "init FFmpeg success";
        emit initAudioOutput(audioCodecContext->sample_rate, audioCodecContext->ch_layout.nb_channels);
        emit initVideoThread(videoCodecContext, hw_device_type, time_base_q2d);
        emit initVideoOutput(hw_device_type);
    }
    else
    {
        isInitSuccess = false;
        qDebug() << "init FFmpeg failed";
    }
    //     init_AudioOutput();
}

int64_t Decode::getAudioFrameCount() const
{
    int64_t ret = -1;
    if (audioStreamIndex == -1)
        ret = formatContext->streams[audioStreamIndex]->nb_frames;
    qDebug() << "audioFrames:" << ret;
    return ret;
}

int64_t Decode::getVideoFrameCount() const
{
    int64_t ret = -1;
    if (videoStreamIndex == -1)
        ret = formatContext->streams[videoStreamIndex]->nb_frames;
    qDebug() << "videoFrames:" << ret;
    return ret;
}

int64_t Decode::getDuration() const
{
    // 流的总时长 ms
    int64_t ret = (formatContext->duration / (int64_t)(AV_TIME_BASE / 1000));
    qDebug() << "duration: " << ret / 1000 / 3600 << ":" << ret / 1000 / 60 << ":" << ret / 1000 % 60;
    return ret;
}

QString Decode::getSupportedHwDecoderNames() const
{
    QString ret;
    AVBufferRef *hw_device_ctx = nullptr;
    for (auto device : devices)
    {
        if (av_hwdevice_ctx_create(&hw_device_ctx, device, nullptr, nullptr, 0) < 0)
            continue;
        else
            av_buffer_unref(&hw_device_ctx);
        qDebug() << " avail devices: " << av_hwdevice_get_type_name(device);
    }
    return QString();
}

void Decode::onAudioDataUsed()
{
    isAudioPacketEmpty = true;
}

void Decode::onVideoDataUsed()
{
    isVideoPacketEmpty = true;
}

void Decode::onVideoQueueStatus(int status)
{
    videoQueueStatus = status;
}

void Decode::setCurFrame(int64_t _curPts)
{
    curPts = _curPts;
    qDebug() << "curPts: " << curPts;
    av_seek_frame(formatContext, audioStreamIndex, curPts / 1000.0 / time_base_q2d, AVSEEK_FLAG_FRAME);
}

int Decode::initFFmpeg(const QString &filePath)
{
    try
    {
        formatContext = avformat_alloc_context();

        if (avformat_open_input(&formatContext, filePath.toUtf8().constData(), nullptr, nullptr) != 0)
        {
            throw OPEN_STREAM_ERROR;
        }

        if (avformat_find_stream_info(formatContext, nullptr) < 0)
        {
            throw FIND_INFO_ERROR;
        }

        // Find the audio stream
        const AVCodec *videoCodec = nullptr;
        const AVCodec *audioCodec = nullptr;
        // 找到指定流类型的流信息，并且初始化codec(如果codec没有值)，返回流索引
        videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
        audioStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &audioCodec, 0);

        if (audioStreamIndex == -1 && videoStreamIndex == -1)
        {
            throw FIND_STREAM_ERROR;
        }

        if (audioStreamIndex == -1)
            mediaType = ONLY_VIDEO;
        else if (videoStreamIndex == -1)
            mediaType = ONLY_AUDIO;
        else
            mediaType = MULTI_AUDIO_VIDEO;

        if (audioStreamIndex != -1)
        { // Initialize audio codec context
            if (initCodec(&audioCodecContext, audioStreamIndex, audioCodec) < 0)
            {
                throw INIT_AUDIO_CODEC_CONTEXT_ERROR;
            }

            // Initialize resampler context
            // 成功返回 0, 错误时返回一个负的 AVERROR code
            // 错误时, SwrContext 将被释放 并且 *ps(即传入的swrContext) 被置为空
            AVChannelLayout ac_ch_ly = AV_CHANNEL_LAYOUT_STEREO;
            if (0 != swr_alloc_set_opts2(&swrContext, &ac_ch_ly, AV_SAMPLE_FMT_S16, audioCodecContext->sample_rate, &audioCodecContext->ch_layout, audioCodecContext->sample_fmt, audioCodecContext->sample_rate, 0, nullptr))
            {
                throw INIT_RESAMPLER_CONTEXT_ERROR;
            }
            if (!swrContext || swr_init(swrContext) < 0)
            {
                throw INIT_SW_RENDERER_CONTEXT;
            }
        }

        if (videoStreamIndex != -1)
        { // Initialize video codec context
          // 根据解码器获取支持此解码方式的硬件加速计
          // 所有支持的硬件解码器保存在AVCodec的hw_configs变量中。对于硬件编码器来说又是单独的AVCodec
            if (!devices.isEmpty())
            {
                const AVCodecHWConfig *config = nullptr; // 硬解码器
                for (int i = 0;; i++)
                {
                    config = avcodec_get_hw_config(videoCodec, i);
                    if (config == nullptr)
                        break;

                    // 可能一个解码器对应着多个硬件加速方式，所以这里将其挑选出来
                    if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                        devices.contains(config->device_type) &&
                        config->pix_fmt != AV_PIX_FMT_NONE)
                    {
                        qDebug() << "config->pix_fmt: " << av_get_pixelformat_name(config->pix_fmt);
                        qDebug() << "config->device_type: " << av_hwdevice_get_type_name(config->device_type);
                        if (av_hwdevice_ctx_create(&hw_device_ctx, config->device_type, nullptr, nullptr, 0) < 0)
                        {
                            devices.removeOne(config->device_type);
                        }
                        else
                        {
                            hw_device_pixel = config->pix_fmt;
                            hw_device_type = config->device_type;
                            break;
                        }
                    }
                    else
                        qDebug() << "unuseful config->device_type: " << av_hwdevice_get_type_name(config->device_type);
                }
            }
            if (initCodec(&videoCodecContext, videoStreamIndex, videoCodec) < 0)
            {
                throw INIT_VIDEO_CODEC_CONTEXT_ERROR;
            }
        }

        // Initialize converted audio buffer
        convertedAudioBuffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
        time_base_q2d = av_q2d(formatContext->streams[audioStreamIndex]->time_base);
        qDebug() << "time_base_q2d: " << time_base_q2d;
    }
    catch (FFMPEG_INIT_ERROR error)
    {
        clean();
        debugError(error);
        return error;
    }
    return NO_ERROR;
}

int Decode::initCodec(AVCodecContext **codecContext, int streamIndex, const AVCodec *codec)
{
    *codecContext = avcodec_alloc_context3(codec);
    AVStream *stream = formatContext->streams[streamIndex];

    avcodec_parameters_to_context(*codecContext, stream->codecpar);

    // 如果是视频流，并且支持硬件加速，则设置硬件加速
    if (formatContext->streams[streamIndex]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        hw_device_type != AV_HWDEVICE_TYPE_NONE &&
        hw_device_ctx != nullptr)
    {
        (*codecContext)->opaque = this;
        (*codecContext)->get_format = getHwFormat;
        (*codecContext)->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_buffer_unref(&hw_device_ctx);
    }
    qDebug() << "(*codecContext)->codec_id: " << (*codecContext)->codec_id;
    return avcodec_open2(*codecContext, codec, nullptr);
}

void Decode::clean()
{
    audioStreamIndex = -1;
    videoStreamIndex = -1;
    isAudioPacketEmpty = true;
    isVideoPacketEmpty = true;
    mediaType = UNKNOWN;
    time_base_q2d = 0;
    curPts = 0;
    hw_device_pixel = AV_PIX_FMT_NONE;

    // 倒着清理
    if (convertedAudioBuffer)
    {
        av_free(convertedAudioBuffer);
        convertedAudioBuffer = nullptr;
    }

    if (swrContext)
    {
        swr_free(&swrContext);
        swrContext = nullptr;
    }

    if (videoCodecContext)
    {
        avcodec_free_context(&videoCodecContext);
        videoCodecContext = nullptr;
    }

    if (audioCodecContext)
    {
        avcodec_free_context(&audioCodecContext);
        audioCodecContext = nullptr;
    }

    if (formatContext)
    {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
}

void Decode::decodePacket()
{
    if (*m_type != CONTL_TYPE::PLAY)
        return;

    AVFrame *frame = nullptr; // 帧
    frame = av_frame_alloc();

    try
    {
        while (*m_type == CONTL_TYPE::PLAY)
        {
            // qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "decodePacket";
            AVPacket *packet = av_packet_alloc();
            if (av_read_frame(formatContext, packet) < 0)
                throw CONTL_TYPE::END;
            if (packet->stream_index == audioStreamIndex)
            {
                continue;
                while (!isAudioPacketEmpty)
                {
                    if (*m_type != CONTL_TYPE::PLAY)
                        throw *m_type;
                    QThread::msleep(10);
                }
                // 将音频帧发送到音频解码器
                if (avcodec_send_packet(audioCodecContext, packet) == 0)
                {
                    if (avcodec_receive_frame(audioCodecContext, frame) == 0)
                    {
                        int64_t out_nb_samples = av_rescale_rnd(
                            swr_get_delay(swrContext, frame->sample_rate) + frame->nb_samples,
                            44100,
                            frame->sample_rate,
                            AV_ROUND_UP);

                        // 将音频帧转换为 PCM 格式
                        // convertedSize：转换后音频数据的大小
                        int convertedSize = swr_convert(
                            swrContext,                    // 转换工具?
                            &convertedAudioBuffer,         // 输出
                            out_nb_samples,                // 输出大小
                            (const uint8_t **)frame->data, // 输入
                            frame->nb_samples);            // 输入大小

                        if (convertedSize > 0)
                        {
                            int bufferSize = av_samples_get_buffer_size(nullptr,
                                                                        frame->ch_layout.nb_channels,
                                                                        convertedSize,
                                                                        AV_SAMPLE_FMT_S16,
                                                                        1);
                            double framePts = time_base_q2d * 1000 * frame->pts;
                            // 将转换后的音频数据发送到音频播放器
                            isAudioPacketEmpty = false;
                            emit sendAudioData(convertedAudioBuffer, bufferSize, framePts);
                        }
                    }
                    curPts = packet->pts;
                }
            }
            else if (packet->stream_index == videoStreamIndex)
            {
                QThread::msleep(20);
                // while (!isVideoPacketEmpty)
                // {
                //     if (*m_type != CONTL_TYPE::PLAY)
                //         throw *m_type;
                //     QThread::msleep(10);
                // }
                emit sendVideoPacket(packet);
                continue;

                // // 将视频帧发送到视频解码器
                // if (avcodec_send_packet(videoCodecContext, &packet) == 0)
                // {
                //     isVideoPacketEmpty = false;
                //     emit sendVideoPacket();
                //     curPts = packet.pts;
                // }

                // qDebug() << "video   end:" << QDateTime::currentMSecsSinceEpoch();
            }

            // 擦除缓存数据包
            av_packet_unref(packet);
        }
    }
    catch (int controlType)
    {
        if (controlType == CONTL_TYPE::END)
            emit playOver();
        debugPlayerCommand(CONTL_TYPE(controlType));
    }

    qDebug() << "Decode::decodePacket() end";
    if (frame)
        av_frame_free(&frame);
}

void Decode::debugError(FFMPEG_INIT_ERROR error)
{
    switch (error)
    {
    case FFMPEG_INIT_ERROR::OPEN_STREAM_ERROR:
        qDebug() << "open stream error";
        break;
    case FFMPEG_INIT_ERROR::FIND_INFO_ERROR:
        qDebug() << "find info error";
        break;
    case FFMPEG_INIT_ERROR::FIND_STREAM_ERROR:
        qDebug() << "find stream error";
        break;
    case FFMPEG_INIT_ERROR::INIT_VIDEO_CODEC_CONTEXT_ERROR:
        qDebug() << "init video codec context error";
        break;
    case FFMPEG_INIT_ERROR::INIT_AUDIO_CODEC_CONTEXT_ERROR:
        qDebug() << "init audio codec context error";
        break;
    case FFMPEG_INIT_ERROR::INIT_RESAMPLER_CONTEXT_ERROR:
        qDebug() << "init resampler context error";
        break;
    case FFMPEG_INIT_ERROR::INIT_SW_RENDERER_CONTEXT:
        qDebug() << "init sw renderer context error";
        break;
    default:
        break;
    }
}

AVPixelFormat Decode::getHwFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    Decode *pThis = static_cast<Decode *>(ctx->opaque);
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++)
    {
        if (*p == pThis->hw_device_pixel)
        {
            return *p;
        }
    }

    qDebug() << "Failed to get HW surface format.";
    return AV_PIX_FMT_NONE;
}

QString av_get_pixelformat_name(AVPixelFormat format)
{
    QString name;
    switch (format)
    {
    case AV_PIX_FMT_NONE:
        name = "AV_PIX_FMT_NONE";
        break;
    case AV_PIX_FMT_YUV420P:
        name = "AV_PIX_FMT_YUV420P";
        break;
    case AV_PIX_FMT_NV12:
        name = "AV_PIX_FMT_NV12";
        break;
    case AV_PIX_FMT_VAAPI:
        name = "AV_PIX_FMT_VAAPI";
        break;
    case AV_PIX_FMT_VIDEOTOOLBOX:
        name = "AV_PIX_FMT_VIDEOTOOLBOX";
        break;
    case AV_PIX_FMT_MEDIACODEC:
        name = "AV_PIX_FMT_MEDIACODEC";
        break;
    case AV_PIX_FMT_D3D11:
        name = "AV_PIX_FMT_D3D11";
        break;
    case AV_PIX_FMT_OPENCL:
        name = "AV_PIX_FMT_OPENCL";
        break;
    case AV_PIX_FMT_VULKAN:
        name = "AV_PIX_FMT_VULKAN";
        break;
    case AV_PIX_FMT_D3D12:
        name = "AV_PIX_FMT_D3D12";
        break;
    case AV_PIX_FMT_DXVA2_VLD:
        name = "AV_PIX_FMT_DXVA2_VLD";
        break;
    case AV_PIX_FMT_VDPAU:
        name = "AV_PIX_FMT_VDPAU";
        break;
    case AV_PIX_FMT_QSV:
        name = "AV_PIX_FMT_QSV";
        break;
    case AV_PIX_FMT_MMAL:
        name = "AV_PIX_FMT_MMAL";
        break;
    case AV_PIX_FMT_D3D11VA_VLD:
        name = "AV_PIX_FMT_D3D11VA_VLD";
        break;
    case AV_PIX_FMT_CUDA:
        name = "AV_PIX_FMT_CUDA";
        break;

    default:
        name = "value:" + QString::number(format);
        break;
    }
    return name;
}
