// AudioThread.cpp
#include "audioThread.h"

#define MAX_AUDIO_FRAME_SIZE 192000
AudioThread::AudioThread(QObject *parent)
    : QObject(parent)
{
    convertedAudioBuffer = new uint8_t[MAX_AUDIO_FRAME_SIZE];
}

AudioThread::~AudioThread()
{
    clean();
    delete[] convertedAudioBuffer;
}

void AudioThread::onInitAudioOutput(int sampleRate, int channels)
{
    clean();

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

void AudioThread::onGetAudioClock(double *pts)
{ // 参考 https://www.cnblogs.com/wangguchangqing/p/5900426.html 中 获取Audio Clock

    // 输出流中未播放数据大小
    int buf_size = audioOutput->bufferSize();

    // 参考FPS (帧数/秒), 此处为 比特数/秒
    int bytes_per_sec = sample_rate * nb_channels * 2;

    // 当前时间戳 - 输出流中缓存数据 可播放时长
    *pts = (cur_pts - static_cast<double>(buf_size) / bytes_per_sec);
}

void AudioThread::recvAudioData(uint8_t *audioBuffer, int bufferSize, double pts)
{
    // qDebug() << "-----------output start------";
    // qDebug() << "audio thread:" << QThread::currentThreadId();
    memcpy(convertedAudioBuffer, audioBuffer, bufferSize);
    emit audioDataUsed();
    cur_pts = pts;

    while (audioOutput->bytesFree() < bufferSize)
        QThread::msleep(10);
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
    }
}
