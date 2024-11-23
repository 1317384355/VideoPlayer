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
    // uint8_t **pixel = copyData(data, linesize, pixelWidth, pixelHeight);
    uint8_t *pixel = data;
    // qDebug() << "ptr-recvVideoData" << pixel;
    emit videoDataUsed();
    emit getAudioClock(&audioClock);
    int sleepTime = pts - audioClock;
    if (sleepTime > 0)
        QThread::msleep(sleepTime);

    emit sendFrame(pixel, pixelWidth, pixelHeight);
}

uint8_t **VideoThread::copyData(uint8_t **data, int *linesize, int pixelWidth, int pixelHeight)
{
    uint8_t **pixel = new uint8_t *[3];
    int halfWidth = pixelWidth / 2;
    int halfHeight = pixelHeight / 2;
    pixel[0] = new uint8_t[pixelWidth * pixelHeight];
    pixel[1] = new uint8_t[halfWidth * halfHeight];
    pixel[2] = new uint8_t[halfWidth * halfHeight];
    for (int i = 0; i < pixelHeight; i++)
    {
        memcpy(pixel[0] + i * pixelWidth, data[0] + i * 1920, static_cast<size_t>(pixelWidth));
    }

    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(pixel[1] + i * halfWidth, data[1] + i * 960, static_cast<size_t>(halfWidth));
    }
    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(pixel[2] + i * halfWidth, data[2] + i * 960, static_cast<size_t>(halfWidth));
    }
    return pixel;
}
