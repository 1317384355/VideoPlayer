#include "videoThread.h"
#include <QDateTime>
#include <QImage>
#include <QPixmap>
#include <QThread>

using namespace std;

VideoThread::VideoThread(QObject *parent)
    : QObject(parent)
{
}

VideoThread::~VideoThread()
{
}

void VideoThread::recvVideoData(uint8_t *data, int pixelWidth, int pixelHeight, double pts)
{
    uint8_t *pixel = data;

    emit videoDataUsed();
    emit getAudioClock(&audioClock);

    int sleepTime = pts - audioClock;
    if (sleepTime > 0)
        QThread::msleep(sleepTime);

    emit sendFrame(pixel, pixelWidth, pixelHeight);
}