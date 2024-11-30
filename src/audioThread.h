#pragma once

#include <QAudioOutput>
#include <QDebug>
#include <QIODevice>
#include <QThread>

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

    // 音频输出设备初始化
    void onInitAudioOutput(int sampleRate, int channels);

    // 获取音频时钟(必须用Qt::DirectConnection连接)
    void onGetAudioClock(double *pts);

private:
    QAudioOutput *audioOutput = nullptr;
    QIODevice *outputDevice = nullptr; // 音频输出流

    uint8_t *convertedAudioBuffer = nullptr;

    int sample_rate = -1;
    int nb_channels = -1;

    int lastPtsSeconds = 0;
    double curPtsMs = 0; // 当前包的时间戳(单位ms)

    // 输出音频帧
    void outputAudioFrame(uint8_t *audioBuffer, int bufferSize);

    void clean();

public:
    explicit AudioThread(QObject *parent = nullptr);
    ~AudioThread();
};
