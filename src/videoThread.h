#pragma once
#include "playerCommand.h"
#include <QObject>

class VideoThread : public QObject
{
    Q_OBJECT
signals:
    void videoDataUsed();

    // 发送帧数据(当前帧进度, 当前帧)
    void sendFrame(uint8_t **pixelData, int pixelWidth, int pixelHeight);

    void getAudioClock(double *pts);
public slots:
    void recvVideoData(uint8_t **data, int *linesize, int pixelWidth, int pixelHeight, double pts);

private:
    double lastPts = 0; // 上一个包的时间戳(单位ms)
    double audioClock;

    uint8_t **copyData(uint8_t **data, int *linesize, int pixelWidth, int pixelHeight);

public:
    VideoThread(QObject *parent = nullptr);
    ~VideoThread();
};
