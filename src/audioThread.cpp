// AudioThread.cpp
#include "audioThread.h"

#define MAX_AUDIO_FRAME_SIZE 192000
AudioThread::AudioThread(int *_type, QTime *_time) : m_type(_type), m_time(_time),
                                                     formatContext(nullptr),
                                                     codecContext(nullptr),
                                                     frame(nullptr),
                                                     swrContext(nullptr),
                                                     convertedAudioBuffer(nullptr),
                                                     audioOutput(nullptr),
                                                     outputDevice(nullptr),
                                                     audioStreamIndex(-1)
{
    avformat_network_init(); // Initialize FFmpeg network components
    m_thread = new QThread;
    this->moveToThread(m_thread);
    connect(this, &AudioThread::startPlay, this, &AudioThread::runPlay);
    m_thread->start();
}

AudioThread::~AudioThread()
{
    cleanupFFmpeg();
}

void AudioThread::setAudioPath(const QString &filePath)
{
    initializeFFmpeg(filePath);
    initializeAudioOutput();
}

qint64 AudioThread::getAudioFrameCount()
{
    qint64 ret = formatContext->streams[audioStreamIndex]->nb_frames;
    qDebug() << "audioRet:" << ret;
    return ret;
}

void AudioThread::runPlay()
{

    int last_ = -1000; // 保证第一帧无需等待
    // Your processing loop can go here
    while (*m_type == CONTL_TYPE::PLAY)
    {
        if (av_read_frame(formatContext, &packet) < 0)
        {
            break;
        }

        if (packet.stream_index == audioStreamIndex)
        {
            if (avcodec_send_packet(codecContext, &packet) == 0)
            {
                while (avcodec_receive_frame(codecContext, frame) == 0)
                {
                    // Process audio data
                    int convertedSize = swr_convert(swrContext, &convertedAudioBuffer,
                                                    frame->nb_samples * 2,
                                                    (const uint8_t **)frame->data,
                                                    frame->nb_samples);
                    int sleepTime = getElapsed_AndUpdate(last_) - 1;
                    if (sleepTime > 0)
                    {
                        qDebug() << "sleepTime:" << sleepTime;
                        QThread::msleep(15);
                    }
                    outputDevice->write(reinterpret_cast<const char *>(convertedAudioBuffer),
                                        convertedSize * 4); // Assuming 16-bit audio
                }
            }
        }

        // 擦除缓存数据包
        av_packet_unref(&packet);
    }
}

void AudioThread::setCurFrame(int _curFrame)
{
}

inline int AudioThread::getElapsed_AndUpdate(int &last)
{
    int cur_pts = av_q2d(formatContext->streams[audioStreamIndex]->time_base) * 1000 * frame->pts;
    int cur = m_time->msecsSinceStartOfDay();
    int ret = (cur_pts - last_pts) - ((cur - last) > 0 ? (cur - last) : 0);
    last_pts = cur_pts;
    last = cur;
    return ret;
}

int AudioThread::initializeFFmpeg(const QString &filePath)
{
    int error = 0;

    do
    {
        formatContext = avformat_alloc_context();
        if (avformat_open_input(&formatContext, filePath.toUtf8().constData(),
                                nullptr, nullptr) != 0)
        {
            error = 1;
            break;
        }

        if (avformat_find_stream_info(formatContext, nullptr) < 0)
        {
            error = 2;
            break;
        }

        // Find the audio stream
        // av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        for (int i = 0; i < formatContext->nb_streams; i++)
        {
            if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audioStreamIndex = i;
                break;
            }
        }

        if (audioStreamIndex == -1)
        {
            error = 3;
            break;
        }

        // Initialize audio codec context
        codecContext = avcodec_alloc_context3(nullptr);
        avcodec_parameters_to_context(codecContext, formatContext->streams[audioStreamIndex]->codecpar);
        if (avcodec_open2(codecContext, avcodec_find_decoder(codecContext->codec_id), nullptr) < 0)
        {
            error = 4;
            break;
        }

        // Initialize audio frame
        frame = av_frame_alloc();

        // Initialize resampler context
        // 成功返回 0, 错误时返回一个负的 AVERROR code
        // 错误时, SwrContext 将被释放 并且 *ps(即传入的swrContext) 被置为空
        AVChannelLayout ac_ch_ly = AV_CHANNEL_LAYOUT_STEREO;
        if (0 != swr_alloc_set_opts2(&swrContext,
                                     &ac_ch_ly, AV_SAMPLE_FMT_S16, codecContext->sample_rate,
                                     &codecContext->ch_layout, codecContext->sample_fmt, codecContext->sample_rate,
                                     0, nullptr))
        {
            error = 5;
            break;
        }
        if (!swrContext || swr_init(swrContext) < 0)
        {
            error = 6;
            break;
        }

        // Initialize converted audio buffer
        convertedAudioBuffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
        return error;

    } while (1);

    cleanupFFmpeg();
    return error;
}

// clean And free
void AudioThread::cleanupFFmpeg()
{
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

    if (frame)
    {
        av_frame_free(&frame);
        frame = nullptr;
    }

    if (codecContext)
    {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }

    if (formatContext)
    {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
}

void AudioThread::initializeAudioOutput()
{
    QAudioFormat format;
    format.setSampleRate(codecContext->sample_rate);
    format.setChannelCount(codecContext->ch_layout.nb_channels);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    audioOutput = new QAudioOutput(format);
    outputDevice = audioOutput->start();
}
