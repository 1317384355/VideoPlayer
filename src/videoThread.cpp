#include "videoThread.h"
#include <QDateTime>
#include <QImage>
#include <QPixmap>
#include <QThread>

using namespace std;

VideoThread::VideoThread(QObject *parent)
    : QObject(parent)
{
    videoPacketQueue.reserve(5);
}

VideoThread::~VideoThread()
{
}

void VideoThread::onInitVideoThread(AVCodecContext *codecContext, int hw_device_type, double time_base_q2d)
{
    this->videoCodecContext = codecContext;
    this->hw_device_type = (AVHWDeviceType)hw_device_type;
    this->time_base_q2d = time_base_q2d;
}

void VideoThread::recvVideoPacket(AVPacket *packet)
{
    videoPacketQueue.append(packet);
    decodeVideoPacket();
}

void VideoThread::decodeVideoPacket()
{
    auto frame = av_frame_alloc();
    while (!videoPacketQueue.isEmpty())
    {
        auto packet = videoPacketQueue.takeFirst();
        if (avcodec_send_packet(videoCodecContext, packet) == 0)
        {
            av_packet_free(&packet);
            if (avcodec_receive_frame(videoCodecContext, frame) == 0)
            {
                uint8_t *pixelData = nullptr;
                double framePts = time_base_q2d * 1000 * frame->pts;

                if (hw_device_type != AV_HWDEVICE_TYPE_NONE)
                    transferDataFromHW(&frame);

                if (frame->format == AV_PIX_FMT_NV12)
                    pixelData = copyNv12Data(frame->data, frame->linesize, frame->width, frame->height);
                else if (frame->format == AV_PIX_FMT_YUV420P)
                    pixelData = copyYuv420pData(frame->data, frame->linesize, frame->width, frame->height);
                else
                {
                    qDebug() << "unsupported video format";
                    break;
                }

                useVideoData(pixelData, frame->width, frame->height, framePts);
            }
        }
    }
    av_frame_free(&frame);
    emit videoDataUsed();
}

void VideoThread::useVideoData(uint8_t *data, int pixelWidth, int pixelHeight, double pts)
{
    emit getAudioClock(audioClock);

    int sleepTime = pts - audioClock;
    // qDebug() << "sleepTime: " << sleepTime
    //          << "pts: " << QString::number(pts, 'f', 3)
    //          << "audioClock: " << QString::number(audioClock, 'f', 3)
    //          << "currentTime: " << QDateTime::currentMSecsSinceEpoch();
    if (sleepTime > 0 && audioClock >= 0.1)
        QThread::msleep(sleepTime);
    emit sendFrame(data, pixelWidth, pixelHeight);
}

void VideoThread::transferDataFromHW(AVFrame **frame)
{
    // 如果采用的硬件加速, 解码后的数据还在GPU中, 所以需要通过av_hwframe_transfer_data将GPU中的数据转移到内存中
    // GPU解码数据格式固定为NV12, 来源: https://blog.csdn.net/qq_23282479/article/details/118993650
    AVFrame *tmp_frame = av_frame_alloc();
    if (0 > av_hwframe_transfer_data(tmp_frame, *frame, 0))
    {
        qDebug() << "av_hwframe_transfer_data fail";
        av_frame_free(&tmp_frame);
        return;
    }
    av_frame_free(frame);
    *frame = tmp_frame;
}

uint8_t *VideoThread::copyNv12Data(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight)
{
    uint8_t *pixel = new uint8_t[pixelWidth * pixelHeight * 3 / 2];
    uint8_t *y = pixel;
    uint8_t *uv = pixel + pixelWidth * pixelHeight;

    int halfHeight = pixelHeight >> 1;
    for (int i = 0; i < pixelHeight; i++)
    {
        memcpy(y + i * pixelWidth, pixelData[0] + i * linesize[0], static_cast<size_t>(pixelWidth));
    }
    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(uv + i * pixelWidth, pixelData[1] + i * linesize[1], static_cast<size_t>(pixelWidth));
    }
    return pixel;
}

uint8_t *VideoThread::copyYuv420pData(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight)
{
    uint8_t *pixel = new uint8_t[pixelHeight * pixelWidth * 3 / 2];
    int halfWidth = pixelWidth >> 1;
    int halfHeight = pixelHeight >> 1;
    uint8_t *y = pixel;
    uint8_t *u = pixel + pixelWidth * pixelHeight;
    uint8_t *v = pixel + pixelWidth * pixelHeight + halfWidth * halfHeight;
    for (int i = 0; i < pixelHeight; i++)
    {
        memcpy(y + i * pixelWidth, pixelData[0] + i * linesize[0], static_cast<size_t>(pixelWidth));
    }
    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(u + i * halfWidth, pixelData[1] + i * linesize[1], static_cast<size_t>(halfWidth));
    }
    for (int i = 0; i < halfHeight; i++)
    {
        memcpy(v + i * halfWidth, pixelData[2] + i * linesize[2], static_cast<size_t>(halfWidth));
    }
    return pixel;
}