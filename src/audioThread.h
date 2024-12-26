#pragma once

#include "decode.h"
#include <QAudioOutput>
#include <QDebug>
#include <QIODevice>
#include <QQueue>
#include <QThread>

// #include "playerCommand.h"

class AudioThread : public QObject
{
    Q_OBJECT
signals:
    void audioDataUsed();

    void audioClockChanged(int pts_seconds);

public slots:
    // 音频输出设备初始化
    void onInitAudioOutput(int sampleRate, int channels);

    void recvAudioPacket(AVPacket *packet);
    void recvAudioBuffer(uint8_t *audioBuffer, int bufferSize, double pts);

    // 获取音频时钟(必须用Qt::DirectConnection连接)
    void onGetAudioClock(double &pts);

private:
    AudioDecoder *audioDecoder{nullptr}; // 音频解码器

    QAudioOutput *audioOutput{nullptr}; // 音频输出
    QIODevice *outputDevice{nullptr};   // 音频输出设备

    uint8_t *convertedAudioBuffer{nullptr};

    double time_base_q2d{0.0}; // 时间基准(秒)

    // 参考FPS (帧数/秒), 此处为 比特数/秒, 采样率 * 通道数 * 每个样本字节数
    int bytes_per_sec = -1;

    int lastPtsSeconds = 0;
    double curPtsMs = 0; // 当前包的时间戳(单位ms)

    QQueue<AVPacket *> audioPacketQueue;

    // 输出音频帧
    void outputAudioFrame(uint8_t *audioBuffer, int bufferSize);

    void clean();

public:
    explicit AudioThread(QObject *parent = nullptr);
    ~AudioThread();

    void setAudioDecoder(AudioDecoder *audioDecoder);
};
