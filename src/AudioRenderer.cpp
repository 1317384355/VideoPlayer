#include "AudioRenderer.h"
#include <QThread>

#define SAMPLE_SIZE 16 // 采样位数/位深
#define MAX_AUDIO_FRAME_SIZE 192000

void AudioRenderer::onInitAudioOutput(int sampleRate, int channels)
{
    clean();

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleSize(SAMPLE_SIZE);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::UnSignedInt);

    audioOutput = new QAudioOutput(format);
    outputDevice = audioOutput->start();
}

void AudioRenderer::onGetAudioClock(double &pts) const
{
    if (audioOutput == nullptr)
        return;

    pts = curPtsMs;
}

inline void AudioRenderer::outputAudioFrame(uint8_t *audioBuffer, int bufferSize)
{
    if (audioOutput == nullptr)
        return;

    outputDevice->write((char *)audioBuffer, bufferSize);
}

inline void AudioRenderer::outputAudioFrame(const QByteArray &audioBuffer)
{
    if (audioOutput == nullptr)
        return;

    outputDevice->write(audioBuffer);
}

void AudioRenderer::clean()
{
    if (audioOutput)
    {
        audioOutput->stop();
        audioOutput->deleteLater();
        audioOutput = nullptr;
        outputDevice = nullptr;
    }
    lastPtsSeconds = 0;
    curPtsMs = 0.001;
}
void AudioRenderer::recvAudioBuffer(uint8_t *buffer, int bufferSize, double pts_ms)
{
    if (audioOutput == nullptr)
        return;

    while (audioOutput->bytesFree() < bufferSize)
        QThread::msleep(10);

    curPtsMs = pts_ms + 0.001;
    int curPtsSeconds = curPtsMs / 1000.0;
    if (curPtsSeconds != lastPtsSeconds)
    {
        lastPtsSeconds = curPtsSeconds;
        emit audioClockChanged(curPtsSeconds);
    }
    outputAudioFrame(buffer, bufferSize);
    delete[] buffer;
}

// void AudioRenderer::recvAudioBuffer(const QByteArray &audioBuffer, double pts_ms)
// {
//     if (audioOutput == nullptr)
//         return;

//     while (audioOutput->bytesFree() < audioBuffer.size())
//         QThread::msleep(10);

//     int curPtsSeconds = pts_ms / 1000.0;
//     if (curPtsSeconds != lastPtsSeconds)
//     {
//         lastPtsSeconds = curPtsSeconds;
//         emit audioClockChanged(curPtsSeconds);
//     }
//     emit audioClockChanged(pts_ms);
//     outputAudioFrame(audioBuffer);
// }

// #define BITS_PER_BYTE 8 // 每个字节多少位
// #define MS_PER_S 1000.0 // 每秒多少毫秒
// void AudioRenderer::onGetAudioClock(double &pts) const
// { // 参考 https://www.cnblogs.com/wangguchangqing/p/5900426.html 中 获取Audio Clock
// // 输出流中未播放数据大小
// int unused_buf_size = this->bufferSize - audioOutput->bytesFree(); // 返回数据为periodSize整数倍, 故无实际运算价值

// double tmp = curEndPtsMs;
// // 当前时间戳 - 输出流中缓存数据 可播放时长
// bytes_per_ms = sampleRate * channels * SAMPLE_SIZE / BITS_PER_BYTE / MS_PER_S; // 比特率(字节/每毫秒)
// pts = (tmp - static_cast<double>(unused_buf_size) / bytes_per_ms);
// }

// void AudioRenderer::recvAudioBuffer(uint8_t *buffer, int bufferSize, double pts)
// {
//     this->audioBuffer.append((char *)buffer, bufferSize);
//     curPtsMs = pts + 0.001;
//     this->audioBuffer.append((char *)buffer, bufferSize);
//     delete[] buffer;

//     if (this->audioBuffer.size() < periodSize)
//         return;

//     double tmp = curEndPtsMs + double(bufferSize / periodSize) / bytes_per_ms;
//     curEndPtsMs = tmp;
//     // qDebug() << "-" << pts << tmp << bufferSize;

//     while (audioBuffer.size() > periodSize)
//     {
//         auto periodBuf = audioBuffer.remove(0, periodSize);

//         while (audioOutput->bytesFree() < periodSize)
//             QThread::msleep(10);

//         // curPtsMs += periodTime_ms;
//         int curPtsSeconds = curPtsMs / 1000.0;

//         if (curPtsSeconds != lastPtsSeconds)
//         {
//             lastPtsSeconds = curPtsSeconds;
//             emit audioClockChanged(curPtsSeconds);
//         }
//         outputAudioFrame(periodBuf);
//     }
// }