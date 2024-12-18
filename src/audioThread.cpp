// AudioThread.cpp
#include "audioThread.h"

#define MAX_AUDIO_FRAME_SIZE 192000
AudioThread::AudioThread(QObject *parent)
    : QObject(parent)
{
    // Initialize converted audio buffer
    convertedAudioBuffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
}

AudioThread::~AudioThread()
{
    clean();
    av_free(convertedAudioBuffer);
}

void AudioThread::onInitAudioThread(AVCodecContext *audioCodecContext, void *swrContext, double time_base_q2d)
{
    clean();
    this->audioCodecContext = audioCodecContext;
    this->swrContext = static_cast<SwrContext *>(swrContext);
    this->time_base_q2d = time_base_q2d;
}

void AudioThread::onInitAudioOutput(int sampleRate, int channels)
{
    sample_rate = sampleRate;
    nb_channels = channels;

    QAudioFormat format;
    format.setSampleRate(sample_rate);
    format.setChannelCount(nb_channels);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    audioOutput = new QAudioOutput(format);
    outputDevice = audioOutput->start();
    emit audioOutputReady();
}

void AudioThread::onGetAudioClock(double &pts)
{ // 参考 https://www.cnblogs.com/wangguchangqing/p/5900426.html 中 获取Audio Clock
    if (audioOutput == nullptr)
        return;

    // 输出流中未播放数据大小
    int buf_size = audioOutput->bufferSize();
    if (buf_size <= 0)
        return;

    // 参考FPS (帧数/秒), 此处为 比特数/秒
    int bytes_per_sec = sample_rate * nb_channels * 2;

    // 当前时间戳 - 输出流中缓存数据 可播放时长
    pts = (curPtsMs - static_cast<double>(buf_size) / bytes_per_sec);
}

void AudioThread::recvAudioPacket(AVPacket *packet)
{
    if (packet == nullptr || audioOutput == nullptr)
        return;

    audioPacketQueue.append(packet);
    decodeAudioPacket();
}

void AudioThread::decodeAudioPacket()
{
    auto frame = av_frame_alloc();
    while (!audioPacketQueue.isEmpty())
    {
        auto packet = audioPacketQueue.takeFirst();
        // 将音频帧发送到音频解码器
        if (avcodec_send_packet(audioCodecContext, packet) == 0)
        {
            av_packet_free(&packet);
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

                    recvAudioData(convertedAudioBuffer, bufferSize, framePts);
                }
            }
        }
    }
    av_frame_free(&frame);
    emit audioDataUsed();
}

void AudioThread::recvAudioData(uint8_t *audioBuffer, int bufferSize, double pts)
{
    // qDebug() << "-----------output start------";
    memcpy(convertedAudioBuffer, audioBuffer, bufferSize);

    curPtsMs = pts;
    int curPtsSeconds = pts / 1000;
    // 将curPtsSeconds转为时:分:秒的字符串
    QString ptsTime = QTime::fromMSecsSinceStartOfDay(pts).toString("hh:mm:ss");

    while (audioOutput->bytesFree() < bufferSize)
        QThread::msleep(10);

    if (curPtsSeconds != lastPtsSeconds)
    {
        lastPtsSeconds = curPtsSeconds;
        emit audioClockChanged(curPtsSeconds, ptsTime);
    }
    outputAudioFrame(convertedAudioBuffer, bufferSize);
    // qDebug() << "-----------output over------";
}

void AudioThread::outputAudioFrame(uint8_t *audioBuffer, int bufferSize)
{
    outputDevice->write(reinterpret_cast<const char *>(convertedAudioBuffer),
                        bufferSize);
}

void AudioThread::clean()
{
    if (audioOutput)
    {
        audioOutput->stop();
        outputDevice = nullptr;
        audioOutput->deleteLater();
        audioOutput = nullptr;

        audioCodecContext = nullptr;

        sample_rate = -1;
        nb_channels = -1;

        lastPtsSeconds = 0;
        curPtsMs = 0;
    }
}
