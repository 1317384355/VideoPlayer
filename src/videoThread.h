#pragma once
#include "playerCommand.h"
#include <QObject>

class VideoThread : public QObject
{
    Q_OBJECT
signals:
    void videoDataUsed();

    // 发送当前帧画面
    void sendFrame(uint8_t *pixelData, int pixelWidth, int pixelHeight);

    void getAudioClock(double *pts);
public slots:
    void recvVideoData(uint8_t *data, int pixelWidth, int pixelHeight, double pts);

private:
    // double lastPtsMs = 0;     // 上一个包的时间戳(单位ms)
    double audioClock;

public:
    VideoThread(QObject *parent = nullptr);
    ~VideoThread();
};
