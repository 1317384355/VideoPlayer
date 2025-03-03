#pragma once
#include <QObject>

class VideoWaiter : public QObject
{
    Q_OBJECT
signals:
    // 发送当前帧画面
    void sendFrame(uint8_t *pixelData, int pixelWidth, int pixelHeight);

    // 通过在信号连接时使用关键词Qt::DirectConnection, 来实现在video线程调用audio线程函数并获取数据
    void getAudioClock(double &pts);
public slots:
    void recvVideoFrame(uint8_t *data, int pixelWidth, int pixelHeight, double pts);

private:
    // double lastPtsMs = 0;     // 上一个包的时间戳(单位ms)
    double audioClock;

public:
    VideoWaiter(QObject *parent = nullptr) : QObject(parent) {}
    ~VideoWaiter() {}
};
