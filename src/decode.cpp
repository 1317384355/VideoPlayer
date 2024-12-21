#include "decode.h"
#include <QImage>
#include <QPixmap>
#define MAX_AUDIO_FRAME_SIZE 192000
QString av_get_pixelformat_name(AVPixelFormat format);

Decode::Decode(const int *_type, QObject *parent)
    : m_type(_type),
      QObject(parent),
      formatContext(nullptr),
      mediaType(UNKNOWN)
{
    avformat_network_init(); // Initialize FFmpeg network components
    audioDecoder = new AudioDecoder(this);
    audioDecoder->convertedAudioBuffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
    videoDecoder = new VideoDecoder(this);

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
    av_free(audioDecoder->convertedAudioBuffer);
}

void Decode::setVideoPath(const QString &filePath)
{
    clean();
    if (NO_ERROR == initFFmpeg(filePath))
    {
        qDebug() << "init FFmpeg success";
    }
    else
    {
        qDebug() << "init FFmpeg failed";
    }
}

int64_t Decode::getAudioFrameCount() const
{
    if (audioStreamIndex == -1)
        return -1;

    int64_t ret = formatContext->streams[audioStreamIndex]->nb_frames;
    qDebug() << "audioFrames:" << ret;
    return ret;
}

int64_t Decode::getVideoFrameCount() const
{
    if (videoStreamIndex == -1)
        return -1;

    int64_t ret = formatContext->streams[videoStreamIndex]->nb_frames;
    qDebug() << "videoFrames:" << ret;
    return ret;
}

int64_t Decode::getDuration() const
{
    if (formatContext == nullptr)
        return -1;

    // 流的总时长 ms
    int64_t ret = (formatContext->duration / (int64_t)(AV_TIME_BASE / 1000));
    qDebug() << "duration: " << ret / 1000 / 3600 << ":" << ret / 1000 / 60 << ":" << ret / 1000 % 60;
    return ret;
}

QList<QString> Decode::getSupportedHwDecoderNames()
{
    QList<QString> ret;
    AVBufferRef *hw_device_ctx = nullptr;
    for (auto device : devices)
    {
        if (av_hwdevice_ctx_create(&hw_device_ctx, device, nullptr, nullptr, 0) < 0)
        {
            devices.removeOne(device);
            continue;
        }

        av_buffer_unref(&hw_device_ctx);
        QString name = av_hwdevice_get_type_name(device);
        ret.append(name);
        qDebug() << " avail devices: " << name;
    }
    return ret;
}

bool Decode::resume()
{
    if (formatContext == nullptr)
        return false;

    clearPacketQueue();
    av_seek_frame(formatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
    return true;
}
void Decode::setCurFrame(int64_t curPts_s)
{
    if (formatContext == nullptr)
        return;

    clearPacketQueue();
    int curPts_ms = curPts_s * 1000;
    int64_t timestamp = curPts_ms / audioDecoder->time_base_q2d_ms;
    qDebug() << "curPts_ms: " << curPts_ms << "timestamp :" << timestamp;
    av_seek_frame(formatContext, audioStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
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

        audioStreamIndex = initAudioDecoder();
        videoStreamIndex = initVideoDecoder();
        qDebug() << "audioStreamIndex: " << audioStreamIndex << "videoStreamIndex: " << videoStreamIndex;
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
    }
    catch (FFMPEG_INIT_ERROR error)
    {
        clean();
        debugError(error);
        return error;
    }
    return NO_ERROR;
}

int Decode::initAudioDecoder()
{
    auto &audioCodecContext = audioDecoder->codecContext;
    auto &swrContext = audioDecoder->swrContext;
    auto &audioStreamIndex = audioDecoder->audioStreamIndex;
    auto &time_base_q2d_ms = audioDecoder->time_base_q2d_ms;

    const AVCodec *audioCodec = nullptr;
    // 找到指定流类型的流信息，并且初始化codec(如果codec没有值)，返回流索引
    audioStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &audioCodec, 0);

    if (audioStreamIndex == AVERROR_STREAM_NOT_FOUND)
        return -1;
    else if (audioStreamIndex == AVERROR_DECODER_NOT_FOUND)
        throw FIND_AUDIO_DECODER_ERROR;

    if (initCodec(&audioCodecContext, audioStreamIndex, audioCodec) < 0)
        throw INIT_AUDIO_CODEC_CONTEXT_ERROR;

    // Initialize resampler context
    // 错误时, SwrContext 将被释放 并且 *ps(即传入的swrContext) 被置为空
    AVChannelLayout ac_ch_ly = AV_CHANNEL_LAYOUT_STEREO;
    if (0 != swr_alloc_set_opts2(&swrContext,
                                 &ac_ch_ly, AV_SAMPLE_FMT_S16,
                                 audioCodecContext->sample_rate,
                                 &audioCodecContext->ch_layout,
                                 audioCodecContext->sample_fmt,
                                 audioCodecContext->sample_rate,
                                 0, nullptr))
    {
        throw INIT_RESAMPLER_CONTEXT_ERROR;
    }

    if (!swrContext || swr_init(swrContext) < 0)
        throw INIT_SW_RENDERER_CONTEXT;

    time_base_q2d_ms = av_q2d(formatContext->streams[audioStreamIndex]->time_base) * 1000;

    emit initAudioOutput(audioDecoder->codecContext->sample_rate, audioDecoder->codecContext->ch_layout.nb_channels);
    return audioStreamIndex;
}

int Decode::initVideoDecoder()
{
    auto &videoCodecContext = videoDecoder->codecContext;
    auto &videoStreamIndex = videoDecoder->videoStreamIndex;
    auto &time_base_q2d_ms = videoDecoder->time_base_q2d_ms;
    auto &hw_device_ctx = videoDecoder->hw_device_ctx;
    auto &hw_device_pix_fmt = videoDecoder->hw_device_pix_fmt;
    auto &hw_device_type = videoDecoder->hw_device_type;

    const AVCodec *videoCodec = nullptr;
    // 找到指定流类型的流信息，并且初始化codec(如果codec没有值)，返回流索引
    videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
    if (videoStreamIndex == AVERROR_STREAM_NOT_FOUND)
        return -1;
    else if (videoStreamIndex == AVERROR_DECODER_NOT_FOUND)
        throw FIND_VIDEO_DECODER_ERROR;

    // 根据解码器获取支持此解码方式的硬件加速计
    // 所有支持的硬件解码器保存在AVCodec的hw_configs变量中。对于硬件编码器来说又是单独的AVCodec
    if (!devices.isEmpty())
        initHwdeviceCtx(videoCodec, devices, hw_device_pix_fmt, hw_device_type, &hw_device_ctx);

    if (initCodec(&videoCodecContext, videoStreamIndex, videoCodec) < 0)
        throw INIT_VIDEO_CODEC_CONTEXT_ERROR;

    time_base_q2d_ms = av_q2d(formatContext->streams[videoStreamIndex]->time_base) * 1000;

    emit initVideoOutput(videoDecoder->hw_device_type);
    return videoStreamIndex;
}

void Decode::initHwdeviceCtx(const AVCodec *videoCodec, QList<AVHWDeviceType> &devices, AVPixelFormat &hw_device_pix_fmt, AVHWDeviceType &hw_device_type, AVBufferRef **hw_device_ctx)
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
            if (av_hwdevice_ctx_create(hw_device_ctx, config->device_type, nullptr, nullptr, 0) < 0)
            {
                devices.removeOne(config->device_type);
            }
            else
            {
                hw_device_pix_fmt = config->pix_fmt;
                hw_device_type = config->device_type;
                break;
            }
        }
        else
            qDebug() << "unuseful config->device_type: " << av_hwdevice_get_type_name(config->device_type);
    }
}

int Decode::initCodec(AVCodecContext **codecContext, int streamIndex, const AVCodec *codec)
{
    *codecContext = avcodec_alloc_context3(codec);
    AVStream *stream = formatContext->streams[streamIndex];

    avcodec_parameters_to_context(*codecContext, stream->codecpar);

    // 如果是视频流，并且支持硬件加速，则设置硬件加速
    if (formatContext->streams[streamIndex]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        videoDecoder->hw_device_type != AV_HWDEVICE_TYPE_NONE &&
        videoDecoder->hw_device_ctx != nullptr)
    {
        (*codecContext)->opaque = &this->videoDecoder->hw_device_pix_fmt;
        (*codecContext)->get_format = getHwFormat;
        (*codecContext)->hw_device_ctx = av_buffer_ref(videoDecoder->hw_device_ctx);
        av_buffer_unref(&videoDecoder->hw_device_ctx);
    }
    qDebug() << "(*codecContext)->codec_id: " << (*codecContext)->codec_id;
    return avcodec_open2(*codecContext, codec, nullptr);
}

void Decode::clean()
{
    clearPacketQueue();
    mediaType = UNKNOWN;

    audioDecoder->clean();
    videoDecoder->clean();

    if (formatContext)
    {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
}

void Decode::clearPacketQueue()
{
    audioDecoder->packetIsUsed = true;
    videoDecoder->packetIsUsed = true;
    while (!audioPacketQueue.isEmpty())
    {
        auto packet = audioPacketQueue.dequeue();
        av_packet_free(&packet);
    }
    while (!videoPacketQueue.isEmpty())
    {
        auto packet = videoPacketQueue.dequeue();
        av_packet_free(&packet);
    }
}

void Decode::decodePacket()
{
    if (*m_type != CONTL_TYPE::PLAY)
        return;

    try
    {
        switch (mediaType)
        {
        // case ONLY_VIDEO:
        //     decodeVideo();
        //     break;
        // case ONLY_AUDIO:
        //     decodeAudio();
        //     break;
        case MULTI_AUDIO_VIDEO:
            decodeMultMedia();
            break;
        default:
            throw (int)CONTL_TYPE::END;
        }
    }
    catch (int controlType)
    {
        if (controlType == CONTL_TYPE::END)
            emit playOver();
        debugPlayerCommand(CONTL_TYPE(controlType));
    }
}

void Decode::decodeMultMedia()
{
    int lastStreamIndex = -1;
    while (*m_type == CONTL_TYPE::PLAY)
    {
        while (audioDecoder->packetIsUsed == false && videoDecoder->packetIsUsed == false)
        {
            if (*m_type != CONTL_TYPE::PLAY)
                throw *m_type;
            QThread::msleep(10);
        }

        if (audioDecoder->packetIsUsed)
        {
            AVPacket *packet = nullptr;
            if (!audioPacketQueue.isEmpty())
            {
                packet = audioPacketQueue.dequeue();
            }
            else
            {
                packet = av_packet_alloc();
                if (av_read_frame(formatContext, packet) < 0)
                    throw (int)CONTL_TYPE::END;
            }

            if (packet->stream_index == audioStreamIndex)
            {
                // qDebug() << "audioStreamIndex, packet->pts: " << audioDecoder->time_base_q2d_ms * packet->pts;

                audioDecoder->packetIsUsed = false;
                emit sendAudioPacket(packet);
                // emit decodeAudioPacket();
            }
            else if (packet->stream_index == videoStreamIndex)
            {
                videoPacketQueue.enqueue(packet);
            }
            else
            {
                qDebug() << "in audio unknow stream index: " << packet->stream_index;
                av_packet_free(&packet);
            }
        }
        if (videoDecoder->packetIsUsed)
        {
            AVPacket *packet = nullptr;
            if (!videoPacketQueue.isEmpty())
            {
                packet = videoPacketQueue.dequeue();
            }
            else
            {
                packet = av_packet_alloc();
                if (av_read_frame(formatContext, packet) < 0)
                    throw (int)CONTL_TYPE::END;
            }

            if (packet->stream_index == videoStreamIndex)
            {
                // qDebug() << "videoStreamIndex, packet->pts: " << videoDecoder->time_base_q2d_ms * packet->pts;

                videoDecoder->packetIsUsed = false;
                emit sendVideoPacket(packet);
                // emit decodeVideoPacket();
            }
            else if (packet->stream_index == audioStreamIndex)
            {
                audioPacketQueue.enqueue(packet);
            }
            else
            {
                qDebug() << "in video unknow stream index: " << packet->stream_index;
                av_packet_free(&packet);
            }
        }
    }
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
    AVPixelFormat hw_device_pix_fmt = *(static_cast<AVPixelFormat *>(ctx->opaque));
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++)
    {
        if (*p == hw_device_pix_fmt)
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

void AudioDecoder::clean()
{
    // 倒着清理
    if (swrContext)
        swr_free(&swrContext);

    if (codecContext)
        avcodec_free_context(&codecContext);
}

void AudioDecoder::decodeAudioPacket(AVPacket *packet)
{
    // 将音频帧发送到音频解码器
    if (avcodec_send_packet(codecContext, packet) == 0)
    {
        av_packet_free(&packet);
        auto frame = av_frame_alloc();
        if (avcodec_receive_frame(codecContext, frame) == 0)
        {
            int64_t out_nb_samples = av_rescale_rnd(
                swr_get_delay(swrContext, frame->sample_rate) + frame->nb_samples,
                44100,
                frame->sample_rate,
                AV_ROUND_UP);

            // 将音频帧转换为 PCM 格式
            // convertedSize：转换后音频数据的大小
            int convertedSize = swr_convert(
                swrContext,                          // 转换工具?
                &convertedAudioBuffer,               // 输出
                out_nb_samples,                      // 输出大小
                (const uint8_t *const *)frame->data, // 输入
                frame->nb_samples);                  // 输入大小

            if (convertedSize > 0)
            {
                int bufferSize = av_samples_get_buffer_size(nullptr,
                                                            frame->ch_layout.nb_channels,
                                                            convertedSize,
                                                            AV_SAMPLE_FMT_S16,
                                                            1);
                double framePts = time_base_q2d_ms * frame->pts;
                // 将转换后的音频数据发送到音频播放器

                emit sendAudioBuffer(convertedAudioBuffer, bufferSize, framePts);
                av_frame_free(&frame);
            }
        }
    }
    packetIsUsed = true;
}

void VideoDecoder::clean()
{
    hw_device_pix_fmt = AV_PIX_FMT_NONE;

    sws_freeContext(swsContext);
    swsContext = nullptr;

    if (codecContext)
        avcodec_free_context(&codecContext);
}

void VideoDecoder::decodeVideoPacket(AVPacket *packet)
{
    if (avcodec_send_packet(codecContext, packet) == 0)
    {
        av_packet_free(&packet);
        auto frame = av_frame_alloc();
        if (avcodec_receive_frame(codecContext, frame) == 0)
        {
            double framePts = time_base_q2d_ms * frame->pts;

            if (hw_device_type != AV_HWDEVICE_TYPE_NONE)
                transferDataFromHW(&frame);

            uint8_t *pixelData = nullptr;
            if (frame->format == AV_PIX_FMT_NV12)
                pixelData = copyNv12Data(frame->data, frame->linesize, frame->width, frame->height);
            else if (frame->format == AV_PIX_FMT_YUV420P)
                pixelData = copyYuv420pData(frame->data, frame->linesize, frame->width, frame->height);
            else
                qDebug() << "unsupported video format";

            emit sendVideoFrame(pixelData, frame->width, frame->height, framePts);
            av_frame_free(&frame);
        }
    }
    packetIsUsed = true;
}

void VideoDecoder::transferDataFromHW(AVFrame **frame)
{
    // 如果采用的硬件加速, 解码后的数据还在GPU中, 所以需要通过av_hwframe_transfer_data将GPU中的数据转移到内存中
    // GPU解码数据格式固定为NV12, 来源: https://blog.csdn.net/qq_23282479/article/details/118993650
    AVFrame *tmp_frame = av_frame_alloc();
    if (0 > av_hwframe_transfer_data(tmp_frame, *frame, 0))
    {
        qDebug() << "av_hwframe_transfer_data fail";
        av_frame_free(&tmp_frame);
        return;
    }
    av_frame_free(frame);
    *frame = tmp_frame;
}

uint8_t *VideoDecoder::copyNv12Data(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight)
{
    uint8_t *pixel = new uint8_t[pixelWidth * pixelHeight * 3 / 2];
    uint8_t *y = pixel;
    uint8_t *uv = pixel + pixelWidth * pixelHeight;

    int halfHeight = pixelHeight >> 1;
    for (int i = 0; i < pixelHeight; i++)
    {
        memcpy(y + i * pixelWidth, pixelData[0] + i * linesize[0], static_cast<size_t>(pixelWidth));
    }
    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(uv + i * pixelWidth, pixelData[1] + i * linesize[1], static_cast<size_t>(pixelWidth));
    }
    return pixel;
}

uint8_t *VideoDecoder::copyYuv420pData(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight)
{
    uint8_t *pixel = new uint8_t[pixelHeight * pixelWidth * 3 / 2];
    int halfWidth = pixelWidth >> 1;
    int halfHeight = pixelHeight >> 1;
    uint8_t *y = pixel;
    uint8_t *u = pixel + pixelWidth * pixelHeight;
    uint8_t *v = pixel + pixelWidth * pixelHeight + halfWidth * halfHeight;
    for (int i = 0; i < pixelHeight; i++)
    {
        memcpy(y + i * pixelWidth, pixelData[0] + i * linesize[0], static_cast<size_t>(pixelWidth));
    }
    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(u + i * halfWidth, pixelData[1] + i * linesize[1], static_cast<size_t>(halfWidth));
    }
    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(v + i * halfWidth, pixelData[2] + i * linesize[2], static_cast<size_t>(halfWidth));
    }
    return pixel;
}

void VideoDecoder::initSwsContext(AVPixelFormat srcFormat, AVPixelFormat dstFormat, int pixelWidth, int pixelHeight)
{
    swsContext = sws_getContext(pixelWidth, pixelHeight, srcFormat, pixelWidth, pixelHeight, dstFormat,
                                SWS_BICUBIC, nullptr, nullptr, nullptr);
}

AVFrame *VideoDecoder::transFrameToRGB24(AVFrame *frame, int pixelWidth, int pixelHeight)
{
    if (frame->format == AV_PIX_FMT_RGB24)
        return frame;

    if (swsContext == nullptr)
        initSwsContext((AVPixelFormat)frame->format, AV_PIX_FMT_RGB24, pixelWidth, pixelHeight);

    AVFrame *frameRGB = av_frame_alloc();
    av_image_alloc(frameRGB->data, frameRGB->linesize, pixelWidth, pixelHeight, AV_PIX_FMT_RGB24, 1);
    sws_scale(swsContext, frame->data, frame->linesize, 0, pixelHeight, frameRGB->data, frameRGB->linesize);

    return frameRGB;
}

int VideoDecoder::writeOneFrame(AVFrame *frame, int pixelWidth, int pixelHeight, QString fileName)
{
    if (frame == nullptr)
        return -1;

    if (frame->format != AV_PIX_FMT_RGB24)
    {
        AVFrame *frameRGB = transFrameToRGB24(frame, pixelWidth, pixelHeight);
        QImage(frameRGB->data[0], pixelWidth, pixelHeight, frameRGB->linesize[0], QImage::Format_RGB888).save(fileName);
        av_frame_free(&frameRGB);
    }
    else
    {
        QImage(frame->data[0], pixelWidth, pixelHeight, frame->linesize[0], QImage::Format_RGB888).save(fileName);
    }

    return 0;
}
