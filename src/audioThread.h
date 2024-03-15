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
    // 开始播放
    void runPlay();
public:
    enum FFMPEG_INIT_ERROR{
        NO_ERROR = 0,
        OPEN_STREAM_ERROR,

    };

private:
    AVFormatContext *formatContext; // 用于处理媒体文件格式的结构, 包含了许多用于描述文件格式和元数据的信息
    AVCodecContext *codecContext;
    AVFrame *frame; // 帧
    SwrContext *swrContext;

    uint8_t *convertedAudioBuffer;
    AVPacket packet;
    int audioStreamIndex;

    double curPts;
    double time_base_q2d;

    QThread *m_thread; // 播放线程
    const int *m_type; // 控制播放状态

    QAudioOutput *audioOutput;
    QIODevice *outputDevice; // 音频输出流

    // 音频相关结构体初始化
    int init_FFmpeg(const QString &filePath);
    // 音频输出设备初始化
    void init_AudioOutput();
    void clean();

public:
    explicit AudioThread(const int *_type);
    ~AudioThread();

    void setAudioPath(const QString &filePath);

    bool resume();

    // 得到总音频帧数
    int64_t getAudioFrameCount() const;

    int64_t getAudioDuration() const;

    // 得到当前当前音频的时钟进度, 不可作为槽函数
    double getAudioClock() const;
};
