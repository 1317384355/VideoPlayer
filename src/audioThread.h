#pragma once

#include <QAudioOutput>
#include <QDebug>
#include <QIODevice>
#include <QThread>
#include <QQueue>
#include "decode.h"

#include "playerCommand.h"

class AudioThread : public QObject
{
    Q_OBJECT
signals:
    void audioDataUsed();
    void audioOutputReady();

    void audioClockChanged(int pts_seconds, QString pts_str);

public slots:
    void recvAudioData(uint8_t *audioBuffer, int bufferSize, double pts);
    void recvAudioPacket(AVPacket *packet);
    void decodeAudioPacket();

    // 音频输出设备初始化
    void onInitAudioThread(AVCodecContext *audioCodecContext, void *swrContext, double time_base_q2d);
    void onInitAudioOutput(int sampleRate, int channels);

    // 获取音频时钟(必须用Qt::DirectConnection连接)
    void onGetAudioClock(double &pts);

private:
    AVCodecContext *audioCodecContext{nullptr};
    SwrContext *swrContext{nullptr}; // 音频重采样

    QAudioOutput *audioOutput{nullptr}; // 音频输出
    QIODevice *outputDevice{nullptr};   // 音频输出设备

    uint8_t *convertedAudioBuffer{nullptr};

    double time_base_q2d{0.0}; // 时间基准(秒)

    int sample_rate = -1;
    int nb_channels = -1;

    int lastPtsSeconds = 0;
    double curPtsMs = 0; // 当前包的时间戳(单位ms)

    QQueue<AVPacket *> audioPacketQueue;

    // 输出音频帧
    void outputAudioFrame(uint8_t *audioBuffer, int bufferSize);

    void clean();

    void decodeAudioPackage();

public:
    explicit AudioThread(QObject *parent = nullptr);
    ~AudioThread();
};
