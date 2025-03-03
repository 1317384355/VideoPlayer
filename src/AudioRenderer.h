#pragma once
#include <QAudioOutput>
#include <QIODevice>

class AudioRenderer : public QObject
{
    Q_OBJECT
signals:
    void audioClockChanged(int pts_s);
    // void audioClockChanged(double pts_ms);

public slots:
    // 音频输出设备初始化
    void onInitAudioOutput(int sampleRate, int channels);

    void recvAudioBuffer(uint8_t *audioBuffer, int bufferSize, double pts);
    // void recvAudioBuffer(const QByteArray &audioBuffer, double pts);

    // 获取音频时钟(必须用Qt::DirectConnection连接)
    void onGetAudioClock(double &pts) const;

private:
    QAudioOutput *audioOutput{nullptr}; // 音频输出
    QIODevice *outputDevice{nullptr};   // 音频输出设备

    int lastPtsSeconds = 0;
    double curPtsMs = 0; // 当前包的时间戳(单位ms)

    // 输出音频帧
    void outputAudioFrame(uint8_t *audioBuffer, int bufferSize);
    void outputAudioFrame(const QByteArray &audioBuffer);

    void clean();

public:
    explicit AudioRenderer(QObject *parent = nullptr) : QObject(parent) {}
    ~AudioRenderer() override { clean(); }
};
