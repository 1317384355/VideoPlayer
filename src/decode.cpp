#include "decode.h"
#include "playerCommand.h"
#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QThread>

class AVPacketUniquePtr : public std::unique_ptr<AVPacket, void (*)(AVPacket *)>
{
public:
    AVPacketUniquePtr() : std::unique_ptr<AVPacket, void (*)(AVPacket *)>(av_packet_alloc(), [](AVPacket *p)
                                                                          { av_packet_free(&p); }) {}
    AVPacketUniquePtr(AVPacket *p) : std::unique_ptr<AVPacket, void (*)(AVPacket *)>(p, [](AVPacket *p)
                                                                                     { av_packet_free(&p); }) {}

    // 禁用拷贝
    AVPacketUniquePtr(const AVPacketUniquePtr &) = delete;
    // 允许移动
    AVPacketUniquePtr(AVPacketUniquePtr &&) = default;
};

class AVFrameUniquePtr : public std::unique_ptr<AVFrame, void (*)(AVFrame *)>
{
public:
    AVFrameUniquePtr() : std::unique_ptr<AVFrame, void (*)(AVFrame *)>(av_frame_alloc(), [](AVFrame *p)
                                                                       { av_frame_free(&p); }) {}

    // 禁用拷贝
    AVFrameUniquePtr(const AVFrameUniquePtr &) = delete;
    // 允许移动
    AVFrameUniquePtr(AVFrameUniquePtr &&) = default;

    AVFrameUniquePtr(AVFrame *p) : std::unique_ptr<AVFrame, void (*)(AVFrame *)>(p, [](AVFrame *p)
                                                                                 { av_frame_free(&p); }) {}

    AVFrameUniquePtr &operator=(const AVFrameUniquePtr &) = delete;
    AVFrameUniquePtr &operator=(AVFrameUniquePtr &&other) = default;
    AVFrameUniquePtr &operator=(AVFrame *other) { return *this = AVFrameUniquePtr(other); }
};

#define MAX_AUDIO_FRAME_SIZE 192000
QString av_get_pixelformat_name(AVPixelFormat format);

Decoder::Decoder(const int *_type, QObject *parent)
    : QObject(parent),
      formatContext(nullptr),
      mediaType(UNKNOWN),
      m_type(_type)
{
    avformat_network_init(); // Initialize FFmpeg network components
    audioDecoder = new AudioDecoder(this);
    videoDecoder = new VideoDecoder(this);

    // 遍历出设备支持的硬件类型
    enum AVHWDeviceType print_type = AV_HWDEVICE_TYPE_NONE;
    while ((print_type = av_hwdevice_iterate_types(print_type)) != AV_HWDEVICE_TYPE_NONE)
    {
        qDebug() << "suport devices: " << av_hwdevice_get_type_name(print_type);
        devices.append(print_type);
    }
}

Decoder::~Decoder()
{
}

void Decoder::setVideoPath(const QString &filePath)
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

int64_t Decoder::getAudioFrameCount() const
{
    if (audioStreamIndex == -1)
        return -1;

    int64_t ret = formatContext->streams[audioStreamIndex]->nb_frames;
    qDebug() << "audioFrames:" << ret;
    return ret;
}

int64_t Decoder::getVideoFrameCount() const
{
    if (videoStreamIndex == -1)
        return -1;

    int64_t ret = formatContext->streams[videoStreamIndex]->nb_frames;
    qDebug() << "videoFrames:" << ret;
    return ret;
}

int64_t Decoder::getDuration() const
{
    if (formatContext == nullptr)
        return -1;

    // 流的总时长 ms
    int64_t ret = (formatContext->duration / ((int64_t)AV_TIME_BASE / 1000));
    qDebug() << "duration: " << QString::asprintf("%02d:%02d:%02d", ret / 1000 / 3600, ret / 1000 / 60 % 60, ret / 1000 % 60);
    return ret;
}

QList<QString> Decoder::getSupportedHwDecoderNames()
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

bool Decoder::resume()
{
    if (formatContext == nullptr)
        return false;

    clearPacketQueue();
    av_seek_frame(formatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
    return true;
}

void Decoder::setCurFrame(int64_t curPts_s)
{
    if (formatContext == nullptr)
        return;

    clearPacketQueue();
    int curPts_ms = curPts_s * 1000;
    int64_t timestamp = curPts_ms / defalt_time_base_q2d_ms;
    // qDebug() << "curPts_ms: " << curPts_ms << "timestamp :" << timestamp;
    av_seek_frame(formatContext, defaltStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
}

int Decoder::initFFmpeg(const QString &filePath)
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

        // av_dump_format(formatContext, 0, filePath.toUtf8().constData(), 0); // 打印流信息

        audioStreamIndex = initAudioDecoder();
        videoStreamIndex = initVideoDecoder(devices);
        qDebug() << "audioStreamIndex: " << audioStreamIndex << "videoStreamIndex: " << videoStreamIndex;
        if (audioStreamIndex == -1 && videoStreamIndex == -1)
        {
            throw FIND_STREAM_ERROR;
        }

        if (audioStreamIndex == -1)
            mediaType = ONLY_VIDEO;
        else if (videoStreamIndex == -1 || formatContext->streams[audioStreamIndex]->codecpar->codec_id == AV_CODEC_ID_MP3)
            mediaType = ONLY_AUDIO;
        else
            mediaType = MULTI_AUDIO_VIDEO;

        defaltStreamIndex = (mediaType == ONLY_AUDIO) ? audioStreamIndex : videoStreamIndex;
        defalt_time_base_q2d_ms = (mediaType == ONLY_AUDIO) ? audioDecoder->time_base_q2d_ms : videoDecoder->time_base_q2d_ms;
    }
    catch (FFMPEG_INIT_ERROR error)
    {
        clean();
        debugError(error);
        return error;
    }
    return NO_ERROR;
}

int Decoder::initAudioDecoder()
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

    if (initCodec(&audioCodecContext, formatContext->streams[audioStreamIndex]->codecpar, audioCodec) < 0)
        throw INIT_AUDIO_CODEC_CONTEXT_ERROR;

    // Initialize resampler context
    // 错误时, SwrContext 将被释放 并且 *ps(即传入的swrContext) 被置为空
    if (0 != swr_alloc_set_opts2(&swrContext,
                                 &audioCodecContext->ch_layout, AV_SAMPLE_FMT_S16,
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

    // 音频压缩编码格式
    if (AV_CODEC_ID_AAC == audioCodecContext->codec_id)
        qDebug() << "audio codec:AAC";
    else if (AV_CODEC_ID_MP3 == audioCodecContext->codec_id)
        qDebug() << "audio codec:MP3";
    else
        qDebug() << "audio codec:other; value: " << audioCodecContext->codec_id;

    emit initAudioOutput(audioDecoder->codecContext->sample_rate, audioDecoder->codecContext->ch_layout.nb_channels);
    return audioStreamIndex;
}

int Decoder::initVideoDecoder(QList<AVHWDeviceType> &devices)
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
        initHwdeviceCtx(videoCodec, formatContext->streams[videoStreamIndex]->codecpar->width, devices, hw_device_pix_fmt, hw_device_type, &hw_device_ctx);

    if (initCodec(&videoCodecContext, formatContext->streams[videoStreamIndex]->codecpar, videoCodec) < 0)
        throw INIT_VIDEO_CODEC_CONTEXT_ERROR;

    time_base_q2d_ms = av_q2d(formatContext->streams[videoStreamIndex]->time_base) * 1000;

    if (AV_CODEC_ID_H264 == videoCodec->id)
        qDebug() << "video codec:H264";
    else if (AV_CODEC_ID_HEVC == videoCodec->id)
        qDebug() << "video codec:HEVC";
    else
        qDebug() << "video codec:other; value: " << videoCodec->id;

    emit initVideoOutput(videoDecoder->hw_device_type);
    return videoStreamIndex;
}

void Decoder::initHwdeviceCtx(const AVCodec *videoCodec, int videoWidth, QList<AVHWDeviceType> &devices, AVPixelFormat &hw_device_pix_fmt, AVHWDeviceType &hw_device_type, AVBufferRef **hw_device_ctx)
{
    // 硬解优先选择AV_HWDEVICE_TYPE_D3D12VA, 其次 11, 逻辑待优化
    if (devices.contains(AV_HWDEVICE_TYPE_D3D12VA) && 0 == av_hwdevice_ctx_create(hw_device_ctx, AV_HWDEVICE_TYPE_D3D12VA, nullptr, nullptr, 0))
    {
        hw_device_pix_fmt = AV_PIX_FMT_D3D12;
        hw_device_type = AV_HWDEVICE_TYPE_D3D12VA;
        qDebug() << "hw_device_type: AV_HWDEVICE_TYPE_D3D12VA";
        return;
    }
    else if (devices.contains(AV_HWDEVICE_TYPE_D3D11VA) && 0 == av_hwdevice_ctx_create(hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0))
    {
        hw_device_pix_fmt = AV_PIX_FMT_D3D11;
        hw_device_type = AV_HWDEVICE_TYPE_D3D11VA;
        qDebug() << "hw_device_type: AV_HWDEVICE_TYPE_D3D11VA";
        return;
    }

    const AVCodecHWConfig *config = nullptr; // 硬解码器
    for (int i = 0;; i++)
    {
        config = avcodec_get_hw_config(videoCodec, i);
        if (config == nullptr)
            break;

        // 可能一个解码器对应着多个硬件加速方式，所以这里将其挑选出来
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && devices.contains(config->device_type) && config->pix_fmt != AV_PIX_FMT_NONE)
        {
            qDebug() << "config->pix_fmt: " << av_get_pixelformat_name(config->pix_fmt);
            qDebug() << "config->device_type: " << av_hwdevice_get_type_name(config->device_type);

            if (config->pix_fmt == AV_PIX_FMT_CUDA && videoWidth > 2032)
            { // cuda硬解暂时不支持4k, 有bug不会处理,待后续学习
                qDebug() << "CUDA only support width <= 2032";
                continue;
            }

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
        {
            qDebug() << "unuseful config->device_type: " << av_hwdevice_get_type_name(config->device_type);
        }
    }
}

int Decoder::initCodec(AVCodecContext **codecContext, AVCodecParameters *codecpar, const AVCodec *codec)
{
    *codecContext = avcodec_alloc_context3(codec);

    avcodec_parameters_to_context(*codecContext, codecpar);

    // 如果是视频流，并且支持硬件加速，则设置硬件加速
    if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoDecoder->hw_device_type != AV_HWDEVICE_TYPE_NONE && videoDecoder->hw_device_ctx != nullptr)
    {
        // (*codecContext)->opaque = &this->videoDecoder->hw_device_pix_fmt;
        // (*codecContext)->get_format = getHwFormat;
        (*codecContext)->hw_device_ctx = av_buffer_ref(videoDecoder->hw_device_ctx);
        av_buffer_unref(&videoDecoder->hw_device_ctx);
    }
    qDebug() << "(*codecContext)->codec_id: " << (*codecContext)->codec_id;
    return avcodec_open2(*codecContext, codec, nullptr);
}

void Decoder::clean()
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

void Decoder::clearPacketQueue()
{
    if (videoDecoder && videoDecoder->codecContext)
        avcodec_flush_buffers(videoDecoder->codecContext);

    audioDecoder->lastPts = -1.0;
    videoDecoder->lastPts = -1.0;

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

void Decoder::decodePacket()
{
    if (*m_type != CONTL_TYPE::PLAY)
        return;

    try
    {
        switch (mediaType)
        {
        case ONLY_VIDEO:
            decodeVideo();
            break;
        case ONLY_AUDIO:
            decodeAudio();
            break;
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

void Decoder::decodeAudio()
{
    while (*m_type == CONTL_TYPE::PLAY)
    {
#if true
        emit getCurPts(curPts);
        if (curPts > audioDecoder->lastPts)
        {
            AVPacketUniquePtr packet;
            if (av_read_frame(formatContext, packet.get()) < 0)
            {
                throw (int)CONTL_TYPE::END;
            }

            if (packet->stream_index == audioStreamIndex)
            {
                // qDebug() << "audioStreamIndex, packet->pts: " << audioDecoder->time_base_q2d_ms * packet->pts;
                if (*m_type == CONTL_TYPE::PLAY)
                    audioDecoder->decodeAudioPacket(std::move(packet));
            }
            else
            {
                qDebug() << "in audio unknow stream index: " << packet->stream_index;
            }
        }
#else
        while (audioDecoder->packetIsUsed == false)
        {
            if (*m_type != CONTL_TYPE::PLAY)
                throw *m_type;
            QThread::msleep(10);
        }

        AVPacket *packet = av_packet_alloc();
        if (av_read_frame(formatContext, packet) < 0)
            throw (int)CONTL_TYPE::END;
        if (packet->stream_index == audioStreamIndex)
        {
            audioDecoder->packetIsUsed = false;
            emit sendAudioPacket(packet);
        }
        else
        {
            qDebug() << "unknow stream index: " << packet->stream_index;
            av_packet_free(&packet);
        }
#endif
    }
}

void Decoder::decodeVideo()
{
}

void Decoder::decodeMultMedia()
{
    while (*m_type == CONTL_TYPE::PLAY)
    {
#if true
        emit getCurPts(curPts);
        if (curPts > audioDecoder->lastPts)
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
                {
                    av_packet_free(&packet);
                    throw (int)CONTL_TYPE::END;
                }
            }

            if (packet->stream_index == audioStreamIndex)
            {
                // qDebug() << "audioStreamIndex, packet->pts: " << audioDecoder->time_base_q2d_ms * packet->pts;
                if (*m_type == CONTL_TYPE::PLAY)
                    audioDecoder->decodeAudioPacket(packet);
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

        if (curPts > videoDecoder->lastPts)
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
                {
                    av_packet_free(&packet);
                    throw (int)CONTL_TYPE::END;
                }
            }

            if (packet->stream_index == videoStreamIndex)
            {
                // qDebug() << "videoStreamIndex, packet->pts: " << videoDecoder->time_base_q2d_ms * packet->pts;
                if (*m_type == CONTL_TYPE::PLAY)
                    videoDecoder->decodeVideoPacket(packet);
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
#else
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
#endif
    }
}

void AudioDecoder::clean()
{
    // 倒着清理
    if (swrContext)
        swr_free(&swrContext);

    if (codecContext)
        avcodec_free_context(&codecContext);
}

int AudioDecoder::transferFrameToPCM(AVFrame *frame, uint8_t *dstBuffer)
{
    int64_t out_nb_samples = av_rescale_rnd(
        swr_get_delay(swrContext, frame->sample_rate) + frame->nb_samples,
        44100,
        frame->sample_rate,
        AV_ROUND_UP);

    int convertedSize = swr_convert( // 返回转换出的数据大小
        swrContext,                  // 转换工具
        &dstBuffer,                  // 输出
        out_nb_samples,              // 输出样本数
        frame->data,                 // 输入
        frame->nb_samples);          // 输入样本数

    return convertedSize;
}

void AudioDecoder::decodeAudioPacket(AVPacketUniquePtr packet)
{
    // 将音频帧发送到音频解码器
    if (avcodec_send_packet(codecContext, packet.get()) == 0)
    {
        AVFrameUniquePtr frame;
        if (avcodec_receive_frame(codecContext, frame.get()) == 0)
        {
            std::unique_ptr<uint8_t[]> convertedAudioBuffer(new uint8_t[MAX_AUDIO_FRAME_SIZE]);
            int convertedSize = transferFrameToPCM(frame.get(), convertedAudioBuffer.get());

            if (convertedSize > 0)
            {
                int channels = frame.get()->ch_layout.nb_channels;
                int bufferSize = av_samples_get_buffer_size(nullptr, channels, convertedSize, AV_SAMPLE_FMT_S16, 1);
                double framePts = time_base_q2d_ms * frame.get()->pts;

                lastPts = framePts;
                // 将转换后的音频数据发送到音频播放器
                emit sendAudioBuffer(convertedAudioBuffer.release(), bufferSize, framePts);
            }
            else
            {
                qDebug() << "in audio decode frame error";
            }
        }
        else
        {
            qDebug() << "in audio decode frame error";
        }
    }
}

void VideoDecoder::clean()
{
    hw_device_pix_fmt = AV_PIX_FMT_NONE;

    if (codecContext)
        avcodec_free_context(&codecContext);
}

void VideoDecoder::decodeVideoPacket(AVPacketUniquePtr packet)
{
    if (avcodec_send_packet(codecContext, packet.get()) == 0)
    {
        auto frame = av_frame_alloc();
        int ret = avcodec_receive_frame(codecContext, frame);
        if (ret == 0)
        {
            double framePts = time_base_q2d_ms * frame->pts;

            if (hw_device_type != AV_HWDEVICE_TYPE_NONE)
                transferDataFromHW(&frame);

            uint8_t *pixelData = nullptr;
            switch (frame->format)
            {
            case AV_PIX_FMT_YUV420P:
                pixelData = copyYuv420pData(frame->data, frame->linesize, frame->width, frame->height);
                break;
            case AV_PIX_FMT_NV12:
                pixelData = copyNv12Data(frame->data, frame->linesize, frame->width, frame->height);
                break;
            default:
                pixelData = copyDefaultData(frame);
                break;
            }

            lastPts = framePts;
            emit sendVideoFrame(pixelData, frame->width, frame->height, framePts);
        }
        else if (ret != AVERROR(EAGAIN))
        {
            qDebug() << "avcodec_receive_frame fail: " << ret;
        }
        else
        {
            qDebug() << "avcodec_receive_frame EAGAIN";
        }
        av_frame_free(&frame);
    }
    else
    {
        qDebug() << "avcodec_send_packet fail";
    }
}

void VideoDecoder::transferDataFromHW(AVFrame **frame)
{
    // 如果采用的硬件加速, 解码后的数据还在GPU中, 所以需要通过av_hwframe_transfer_data将GPU中的数据转移到内存中
    // GPU解码数据格式固定为NV12, 来源: https://blog.csdn.net/qq_23282479/article/details/118993650
    AVFrameUniquePtr tmp_frame;
    if (0 > av_hwframe_transfer_data(tmp_frame.get(), *frame, 0))
    {
        qDebug() << "av_hwframe_transfer_data fail";
        return;
    }

    av_frame_free(frame);

    if (tmp_frame->format != AV_PIX_FMT_NV12)
    { // 如果不是 NV12 格式, 则转换为NV12格式, 临时结构, 后续再重新梳理
        *frame = transFrameToDstFmt(tmp_frame.get(), tmp_frame.get()->width, tmp_frame.get()->height, AV_PIX_FMT_NV12);
    }
    else
    {
        *frame = tmp_frame.release();
    }
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

uint8_t *VideoDecoder::copyDefaultData(AVFrame *frame)
{
    AVFrameUniquePtr dstFrame = transFrameToDstFmt(frame, frame->width, frame->height, AV_PIX_FMT_YUV420P);
    return copyYuv420pData(dstFrame.get()->data, frame->linesize, frame->width, frame->height);
}

AVFrame *VideoDecoder::transFrameToRGB24(AVFrame *srcFrame, int pixelWidth, int pixelHeight)
{
    AVFrame *frameRGB = av_frame_alloc();
    if (srcFrame->format == AV_PIX_FMT_RGB24)
    {
        av_frame_copy(frameRGB, srcFrame);
    }
    else
    {
        auto swsContext = sws_getContext(pixelWidth, pixelHeight, (AVPixelFormat)srcFrame->format, pixelWidth, pixelHeight, AV_PIX_FMT_RGB24, SWS_BICUBIC, nullptr, nullptr, nullptr);

        av_image_alloc(frameRGB->data, frameRGB->linesize, pixelWidth, pixelHeight, AV_PIX_FMT_RGB24, 1);
        sws_scale(swsContext, srcFrame->data, srcFrame->linesize, 0, pixelHeight, frameRGB->data, frameRGB->linesize);

        sws_freeContext(swsContext);
    }
    return frameRGB;
}

AVFrame *VideoDecoder::transFrameToDstFmt(AVFrame *srcFrame, int pixelWidth, int pixelHeight, AVPixelFormat dstFormat)
{
    AVFrame *dstFrame = av_frame_alloc();
    SwsContext *swsContext = sws_getContext(pixelWidth, pixelHeight, AVPixelFormat(srcFrame->format),
                                            pixelWidth, pixelHeight, dstFormat,
                                            SWS_BILINEAR, NULL, NULL, NULL);

    int numBytes = av_image_get_buffer_size(dstFormat, pixelWidth, pixelHeight, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(dstFrame->data, dstFrame->linesize, buffer, dstFormat, pixelWidth, pixelHeight, 1);

    sws_scale(swsContext, srcFrame->data, srcFrame->linesize, 0, pixelHeight, dstFrame->data, dstFrame->linesize);

    dstFrame->format = dstFormat;
    dstFrame->width = pixelWidth;
    dstFrame->height = pixelHeight;
    return dstFrame;
}

int VideoDecoder::writeOneFrame(AVFrame *frame, int pixelWidth, int pixelHeight, QString fileName)
{
    if (frame == nullptr)
        return -1;

    if (frame->format != AV_PIX_FMT_RGB24)
    {
        AVFrameUniquePtr frameRGB = transFrameToRGB24(frame, pixelWidth, pixelHeight);
        QImage(frameRGB.get()->data[0], pixelWidth, pixelHeight, frameRGB.get()->linesize[0], QImage::Format_RGB888).save(fileName);
    }
    else
    {
        QImage(frame->data[0], pixelWidth, pixelHeight, frame->linesize[0], QImage::Format_RGB888).save(fileName);
    }

    return 0;
}

// AVPixelFormat Decoder::getHwFormat(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
// {
//     const enum AVPixelFormat *p;
//     AVPixelFormat hw_device_pix_fmt = *(static_cast<AVPixelFormat *>(ctx->opaque));
//     for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++)
//     {
//         if (*p == hw_device_pix_fmt)
//         {
//             return *p;
//         }
//     }

//     qDebug() << "Failed to get HW surface format.";
//     return AV_PIX_FMT_NONE;
// }

void Decoder::debugError(FFMPEG_INIT_ERROR error)
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
