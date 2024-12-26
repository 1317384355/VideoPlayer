// AudioThread.cpp
#include "audioThread.h"

#define MAX_AUDIO_FRAME_SIZE 192000
AudioThread::AudioThread(QObject *parent)
    : QObject(parent)
{
}

AudioThread::~AudioThread()
{
    clean();
}

void AudioThread::setAudioDecoder(AudioDecoder *audioDecoder)
{
    this->audioDecoder = audioDecoder;
    connect(audioDecoder, &AudioDecoder::sendAudioBuffer, this, &AudioThread::recvAudioBuffer);
}

void AudioThread::onInitAudioOutput(int sampleRate, int channels)
{
    clean();
    bytes_per_sec = sampleRate * channels * 2;

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    audioOutput = new QAudioOutput(format);
    outputDevice = audioOutput->start();
}

void AudioThread::onGetAudioClock(double &pts)
{ // 参考 https://www.cnblogs.com/wangguchangqing/p/5900426.html 中 获取Audio Clock
    if (audioOutput == nullptr)
        return;

    // 输出流中未播放数据大小
    int buf_size = audioOutput->bufferSize();

    // 当前时间戳 - 输出流中缓存数据 可播放时长
    pts = (curPtsMs - static_cast<double>(buf_size) / bytes_per_sec);
}

void AudioThread::recvAudioPacket(AVPacket *packet)
{
    if (packet != nullptr && audioOutput != nullptr)
        audioDecoder->decodeAudioPacket(packet);
}

void AudioThread::recvAudioBuffer(uint8_t *audioBuffer, int bufferSize, double pts)
{
    curPtsMs = pts;
    int curPtsSeconds = pts / 1000;

    while (audioOutput->bytesFree() < bufferSize)
        QThread::msleep(10);

    if (curPtsSeconds != lastPtsSeconds)
    {
        lastPtsSeconds = curPtsSeconds;
        emit audioClockChanged(curPtsSeconds);
    }
    outputAudioFrame(audioBuffer, bufferSize);
    // qDebug() << "-----------output over------";
}

void AudioThread::outputAudioFrame(uint8_t *audioBuffer, int bufferSize)
{
    outputDevice->write(reinterpret_cast<const char *>(audioBuffer),
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

        bytes_per_sec = -1;

        lastPtsSeconds = 0;
        curPtsMs = 0;
    }
}
