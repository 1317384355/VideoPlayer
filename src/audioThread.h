#pragma once

#include <QThread>
#include <QIODevice>
#include <QAudioOutput>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

class AudioThread : public QObject
{
    Q_OBJECT
signals:
    void startPlay();

private slots:

public slots:
    // 响应拖动进度条, 跳转到帧并返回这一帧画面
    void setCurFrame(int _curFrame);
    void runPlay();

private:
    AVFormatContext *formatContext;
    AVCodecContext *codecContext;
    AVFrame *frame;
    SwrContext *swrContext;

    uint8_t *convertedAudioBuffer;
    AVPacket packet;
    int audioStreamIndex;

    QThread *m_thread;

    QAudioOutput *audioOutput;
    QIODevice *outputDevice;

    // 音频相关结构体初始化
    void initializeFFmpeg(const QString &filePath);
    void cleanupFFmpeg();

    // 音频输出设备初始化
    void initializeAudioOutput();

public:
    explicit AudioThread();
    ~AudioThread();

    void setAudioPath(const QString &filePath);
    qint64 getAudioFrameCount();
};
