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

void VideoThread::setVideoDecoder(VideoDecoder *decoder)
{
    this->decoder = decoder;
    connect(decoder, &VideoDecoder::sendVideoFrame, this, &VideoThread::recvVideoFrame);
}

void VideoThread::recvVideoPacket(AVPacket *packet)
{
    if (packet != nullptr)
        decoder->decodeVideoPacket(packet);
}

void VideoThread::recvVideoFrame(uint8_t *data, int pixelWidth, int pixelHeight, double pts)
{
    // qDebug() << "recvVideoFrame-pts:" << pts;
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
