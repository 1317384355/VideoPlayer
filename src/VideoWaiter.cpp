#include "VideoWaiter.h"
#include <QThread>

void VideoWaiter::recvVideoFrame(uint8_t *data, int pixelWidth, int pixelHeight, double pts)
{
    emit getAudioClock(audioClock);

    int sleepTime = pts - audioClock;
    // qDebug() << "sleepTime: " << QString("%1").arg(sleepTime, 4, 10, QLatin1Char('0'))
    //          << "pts: " << QString::number(pts, 'f', 3)
    //          << "audioClock: " << QString::number(audioClock, 'f', 3)
    //          << "currentTime: " << QDateTime::currentMSecsSinceEpoch() % 1000000;
    if (sleepTime > 0 && audioClock >= 0.1)
        QThread::msleep(sleepTime);
    emit sendFrame(data, pixelWidth, pixelHeight);
}
