#pragma once
#include "playerCommand.h"
#include "decode.h"
#include <QObject>
#include <QQueue>

class VideoThread : public QObject
{
    Q_OBJECT
signals:
    void videoDataUsed();

    void videoQueueStatus(int status);

    // 发送当前帧画面
    void sendFrame(uint8_t *pixelData, int pixelWidth, int pixelHeight);

    // 通过在信号连接时使用关键词Qt::DirectConnection, 来实现在video线程调用audio线程函数并获取数据
    void getAudioClock(double &pts);
public slots:
    void recvVideoPacket(AVPacket *packet);
    void recvVideoFrame(uint8_t *data, int pixelWidth, int pixelHeight, double pts);

private:
    // double lastPtsMs = 0;     // 上一个包的时间戳(单位ms)
    VideoDecoder *decoder;
    double audioClock;

public:
    VideoThread(QObject *parent = nullptr);
    ~VideoThread();

    void setVideoDecoder(VideoDecoder *decoder);
};
