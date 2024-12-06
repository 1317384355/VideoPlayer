#include "decode.h"
#include <QImage>
#include <QPixmap>
#define MAX_AUDIO_FRAME_SIZE 192000

Decode::Decode(const int *_type, QObject *parent) : m_type(_type), QObject(parent), formatContext(nullptr), videoCodecContext(nullptr), audioCodecContext(nullptr), swrContext(nullptr), convertedAudioBuffer(nullptr), audioStreamIndex(-1), videoStreamIndex(-1), mediaType(UNKNOWN), time_base_q2d(0), curPts(0)
{
    avformat_network_init(); // Initialize FFmpeg network components

    // 遍历出设备支持的硬件类型
    QList<AVHWDeviceType> devices;
    enum AVHWDeviceType print_type = AV_HWDEVICE_TYPE_NONE;
    while ((print_type = av_hwdevice_iterate_types(print_type)) != AV_HWDEVICE_TYPE_NONE)
    {
        devices.append(print_type);
        qDebug() << "suport devices: " << av_hwdevice_get_type_name(print_type);
    }

    if (devices.size() > 0)
        type = devices[0]; // 默认使用第一个设备
    else
        type = AV_HWDEVICE_TYPE_NONE; // 没有设备
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
        emit initVideoOutput(hw_device_pixel);
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
    // while (isIniting)
    // {
    //     if (!isInitSuccess)
    //         return -1;

    //     QThread::msleep(50);
    // }

    // 流的总时长 ms
    int64_t ret = (formatContext->duration / (int64_t)(AV_TIME_BASE / 1000));
    qDebug() << "duration: " << ret / 1000 / 3600 << ":" << ret / 1000 / 60 << ":" << ret / 1000 % 60;
    return ret;
}

void Decode::onAudioDataUsed()
{
    isAudioPacketEmpty = true;
}

void Decode::onVideoDataUsed()
{
    isVideoPacketEmpty = true;
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

            if (type != AV_HWDEVICE_TYPE_NONE)
            {
                for (int i = 0;; i++)
                {
                    config = avcodec_get_hw_config(videoCodec, i);
                    if (config == NULL)
                        break;

                    // 可能一个解码器对应着多个硬件加速方式，所以这里将其挑选出来
                    if (
                        config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type)
                    {
                        hw_device_pixel = config->pix_fmt;
                        qDebug() << "hw_device_pixel: " << Decode::hw_device_pixel;
                        qDebug() << "config->pix_fmt: " << config->pix_fmt;
                    }
                    // 找到了则跳出循环
                    if (hw_device_pixel != AV_PIX_FMT_NONE)
                    {
                        break;
                    }
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

    // 如果是视频流，并且支持硬件加速，则尝试设置硬件加速
    if (formatContext->streams[streamIndex]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && type != AV_HWDEVICE_TYPE_NONE)
    {
        // if(config == nullptr)

        AVBufferRef *hw_device_ctx = nullptr;
        if (av_hwdevice_ctx_create(&hw_device_ctx, type, nullptr, nullptr, 0) < 0)
        {
            qDebug() << "Failed to create device context.";
        }
        else
        {
            (*codecContext)->opaque = this;
            (*codecContext)->get_format = getHwFormat;
            (*codecContext)->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        }
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
            if (av_read_frame(formatContext, &packet) < 0)
                throw CONTL_TYPE::END;

            if (packet.stream_index == audioStreamIndex)
            {
                while (!isAudioPacketEmpty)
                {
                    if (*m_type != CONTL_TYPE::PLAY)
                        throw *m_type;
                    QThread::msleep(10);
                }

                // 将音频帧发送到音频解码器
                if (avcodec_send_packet(audioCodecContext, &packet) == 0)
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
                    curPts = packet.pts;
                }
            }
            else if (packet.stream_index == videoStreamIndex)
            {
                // 将视频帧发送到视频解码器
                if (avcodec_send_packet(videoCodecContext, &packet) == 0)
                {
                    if (avcodec_receive_frame(videoCodecContext, frame) == 0)
                    {
                        while (!isVideoPacketEmpty)
                        {
                            if (*m_type != CONTL_TYPE::PLAY)
                                throw *m_type;
                            QThread::msleep(10);
                        }

                        uint8_t *pixelData = nullptr;

                        if (frame->format == AV_PIX_FMT_CUDA)
                        {
                            // 如果采用的硬件加速, 解码后的数据还在GPU中, 所以需要通过av_hwframe_transfer_data将GPU中的数据转移到内存中
                            // GPU解码数据格式固定为NV12, 来源: https://blog.csdn.net/qq_23282479/article/details/118993650
                            AVFrame *tmp_frame = av_frame_alloc();
                            if (0 > av_hwframe_transfer_data(tmp_frame, frame, 0))
                            {
                                qDebug() << "av_hwframe_transfer_data fail";
                                av_frame_free(&tmp_frame);
                                continue;
                            }
                            av_frame_free(&frame);
                            frame = tmp_frame;

                            pixelData = copyNv12Data(frame->data, frame->linesize, frame->width, frame->height);
                        }
                        else
                        {
                            pixelData = copyYuv420lData(frame->data, frame->linesize, frame->width, frame->height);
                        }
                        double framePts = time_base_q2d * 1000 * frame->pts;

                        isVideoPacketEmpty = false;
                        emit sendVideoData(pixelData, frame->width, frame->height, framePts);
                    }
                    curPts = packet.pts;
                }
            }

            // 擦除缓存数据包
            av_packet_unref(&packet);
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

uint8_t *Decode::copyNv12Data(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight)
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

uint8_t *Decode::copyYuv420lData(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight)
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

// for (int i = 0; i < formatContext->nb_streams; i++) {
//     if (audioStreamIndex != -1 && videoStreamIndex != -1)
//         break;
//     if (audioStreamIndex == -1 && formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
//         audioStreamIndex = i;
//         continue;
//     }
//     if (videoStreamIndex == -1 && formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
//         videoStreamIndex = i;
// }
