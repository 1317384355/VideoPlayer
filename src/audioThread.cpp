// AudioThread.cpp
#include "audioThread.h"

#define MAX_AUDIO_FRAME_SIZE 192000
AudioThread::AudioThread(const int *_type) : m_type(_type),
                                             formatContext(nullptr),
                                             codecContext(nullptr),
                                             frame(nullptr),
                                             swrContext(nullptr),
                                             convertedAudioBuffer(nullptr),
                                             audioOutput(nullptr),
                                             outputDevice(nullptr),
                                             audioStreamIndex(-1),
                                             curPts(0)
{
    avformat_network_init(); // Initialize FFmpeg network components
    m_thread = new QThread;
    this->moveToThread(m_thread);
    connect(this, &AudioThread::startPlay, this, &AudioThread::runPlay);
    m_thread->start();
}

AudioThread::~AudioThread()
{
    clean();
}

bool AudioThread::resume()
{
    curPts = 0;
    av_seek_frame(formatContext, audioStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    return true;
}

void AudioThread::setAudioPath(const QString &filePath)
{
    clean();
    if (NO_ERROR == init_FFmpeg(filePath))
        init_AudioOutput();
}

int64_t AudioThread::getAudioFrameCount() const
{
    int64_t ret = formatContext->streams[audioStreamIndex]->nb_frames;
    qDebug() << "audioFrames:" << ret;
    return ret;
}

int64_t AudioThread::getAudioDuration() const
{
    // 音频流的总时长 ms
    int64_t ret = (formatContext->duration / (int64_t)(AV_TIME_BASE / 1000));
    qDebug() << "audioDuration:" << ret;
    return ret;
}

void AudioThread::runPlay()
{
    while (*m_type == CONTL_TYPE::PLAY)
    {
        // 从输入的音频流中读取一个音频帧
        if (av_read_frame(formatContext, &packet) < 0)
        {
            break;
        }
        // 确保当前 packet 为音频流
        if (packet.stream_index == audioStreamIndex)
        {
            // 将音频帧发送到音频解码器
            if (avcodec_send_packet(codecContext, &packet) == 0)
            {
                curPts++;
                // 接收解码后的音频帧
                while (avcodec_receive_frame(codecContext, frame) == 0)
                {
                    // 将音频帧转换为 PCM 格式
                    // convertedSize：转换后音频数据的大小
                    int convertedSize = swr_convert(swrContext,                    // 转换工具?
                                                    &convertedAudioBuffer,         // 输出
                                                    frame->nb_samples * 2,         // 输出大小
                                                    (const uint8_t **)frame->data, // 输入
                                                    frame->nb_samples);            // 输入大小

                    // 让音频连续按轴连续播放, 根据 QAudioOutput 的剩余缓存空间大小判断是否继续写入
                    while (audioOutput->bytesFree() < convertedSize * 4)
                    { // 不懂为啥是 * 4, 这个数是试出来的
                        if (*m_type != CONTL_TYPE::PLAY)
                            return; // 这个写法可能会导致暂停时再播放时丢失当前帧, 目前不会解决

                        QThread::msleep(10);
                    }
                    // 将音频数据写入输出流, 即播放当前音频帧
                    outputDevice->write(reinterpret_cast<const char *>(convertedAudioBuffer),
                                        convertedSize * 4); // 此处 * 4同上
                }
            }
        }

        // 擦除缓存数据包
        av_packet_unref(&packet);
    }
}

void AudioThread::setCurFrame(int _curPts)
{
    if (*m_type != CONTL_TYPE::PLAY)
    {
        curPts = _curPts;
        qDebug() << "curPts: " << curPts;
        av_seek_frame(formatContext, audioStreamIndex, curPts / 1000.0 / time_base_q2d, AVSEEK_FLAG_FRAME);
    }
}

double AudioThread::getAudioClock() const
{ // 参考 https://www.cnblogs.com/wangguchangqing/p/5900426.html 中 获取Audio Clock

    // 当前帧时间戳
    double cur_pts = time_base_q2d * 1000 * frame->pts;

    // 输出流中未播放数据大小
    int buf_size = audioOutput->bufferSize();
    // 参考FPS (帧数/秒), 此处为 比特数/秒
    int bytes_per_sec = codecContext->sample_rate * codecContext->ch_layout.nb_channels * 2;

    // 当前时间戳 - 输出流中缓存数据 可播放时长
    return (cur_pts - static_cast<double>(buf_size) / bytes_per_sec);
}

int AudioThread::init_FFmpeg(const QString &filePath)
{
    try
    {
        formatContext = avformat_alloc_context();
        if (avformat_open_input(&formatContext, filePath.toUtf8().constData(),
                                nullptr, nullptr) != 0)
        {
            throw OPEN_STREAM_ERROR;
        }

        if (avformat_find_stream_info(formatContext, nullptr) < 0)
        {
            throw FIND_INFO_ERROR;
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
            throw FIND_STREAM_ERROR;
        }

        // Initialize audio codec context
        codecContext = avcodec_alloc_context3(nullptr);
        avcodec_parameters_to_context(codecContext, formatContext->streams[audioStreamIndex]->codecpar);
        if (avcodec_open2(codecContext, avcodec_find_decoder(codecContext->codec_id), nullptr) < 0)
        {
            throw INIT_CODEC_CONTEXT_ERROR;
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
            throw INIT_RESAMPLER_CONTEXT_ERROR;
        }
        if (!swrContext || swr_init(swrContext) < 0)
        {
            throw INIT_SW_RENDERER_CONTEXT;
        }

        // Initialize converted audio buffer
        convertedAudioBuffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);

        time_base_q2d = av_q2d(formatContext->streams[audioStreamIndex]->time_base);
    }
    catch (FFMPEG_INIT_ERROR error)
    {
        clean();
        qDebug() << "there is error with init_FFmpeg(), type:" << error;
        return error;
    }
    return 0;
}

void AudioThread::init_AudioOutput()
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

void AudioThread::clean()
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

    if (audioOutput)
    {
        audioOutput->stop();
        outputDevice = nullptr;
        audioOutput->deleteLater();
    }
}
