#pragma once

#include <QIODevice>
#include <QAudioOutput>
#include "playerCommand.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <QDebug>
#include <QThread>

enum CONTL_TYPE
{
    NONE,
    PLAY,
    PAUSE,
    RESUME,
    END,
};

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
    int last_pts = 0;

    QThread *m_thread;
    const int *m_type;

    QAudioOutput *audioOutput;
    QIODevice *outputDevice;

    // 音频相关结构体初始化
    int initializeFFmpeg(const QString &filePath);
    void cleanupFFmpeg();

    // 音频输出设备初始化
    void initializeAudioOutput();

public:
    explicit AudioThread(const int *_type);
    ~AudioThread();

    void setAudioPath(const QString &filePath);
    qint64 getAudioFrameCount() const;

    // 得到当前当前音频的时钟进度, 不可作为槽函数
    double getAudioClock() const;
};
