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

    void getAudioClock(double *pts);
public slots:
    void onInitVideoThread(AVCodecContext *codecContext, int hw_device_type, double time_base_q2d);

    void recvVideoPacket(AVPacket *packet);
    void decodeVideoPacket();

    void useVideoData(uint8_t *data, int pixelWidth, int pixelHeight, double pts);

private:
    // double lastPtsMs = 0;     // 上一个包的时间戳(单位ms)

    double time_base_q2d;
    AVCodecContext *videoCodecContext;
    enum AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;

    double audioClock;
    QQueue<AVPacket *> videoPacketQueue;

    uint8_t *copyNv12Data(uint8_t **data, int *linesize, int pixelWidth, int pixelHeight);
    uint8_t *copyYuv420pData(uint8_t **data, int *linesize, int pixelWidth, int pixelHeight);

public:
    VideoThread(QObject *parent = nullptr);
    ~VideoThread();
};
