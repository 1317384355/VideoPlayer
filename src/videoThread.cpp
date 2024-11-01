#include "videoThread.h"
#include <opencv2/core/cuda.hpp>
#include <QImage>
#include <QPixmap>

using namespace cv;
using namespace std;

VideoThread::VideoThread(const int *_type, const AudioThread *_audio) : m_type(_type),
                                                                        m_audio(_audio)
{
    m_thread = new QThread;
    this->moveToThread(m_thread);
    connect(this, &VideoThread::startPlay, this, &VideoThread::runPlay);
    m_thread->start();
}

VideoThread::~VideoThread()
{
    qDebug() << "~VideoThread()";
    m_cap.release();
}

bool VideoThread::resume()
{
    curPts = 0;
    return m_cap.set(CAP_PROP_POS_FRAMES, curPts);
}

bool VideoThread::setVideoPath(const QString &path)
{
    curPts = 0;
    return m_cap.open(path.toStdString());
}

double VideoThread::getVideoFrameCount()
{
    double ret = m_cap.get(CAP_PROP_FRAME_COUNT);
    qDebug() << ret;
    return ret;
}

double VideoThread::getVideoDuration()
{
    double ret = m_cap.get(CAP_PROP_FRAME_COUNT) / m_cap.get(CAP_PROP_FPS) * 1000; // 总时长, 单位ms
    qDebug() << m_cap.get(CAP_PROP_FRAME_COUNT) << m_cap.get(CAP_PROP_FPS) << ret;
    return ret;
}

void VideoThread::runPlay()
{
    while (*m_type == CONTL_TYPE::PLAY)
    {
        if (m_cap.read(m_frame))
        {
            if (isSave)
            {
                QString path = QString("./save/%1.png").arg(curPts);
                if (imwrite(path.toStdString(), m_frame) == false)
                    qDebug() << path;
            }
            curPts = m_cap.get(CAP_PROP_POS_MSEC);
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            QPixmap frame = QPixmap::fromImage(QImage(m_frame.data, m_frame.cols, m_frame.rows, m_frame.step, QImage::Format_RGB888));
            int sleepTime = curPts - m_audio->getAudioClock();
            // qDebug() << "video:" << curPts << "audio:" << m_audio->getAudioClock() << "sleepTime:" << sleepTime;
            if (sleepTime > 0)
            {
                QThread::msleep(sleepTime);
            }
            emit sendFrame(curPts, frame);
        }
        else
        {
            break;
        }
    }
}

void VideoThread::setCurFrame(int _curPts)
{
    if (qAbs(_curPts - curPts) < 5)
    {
        return;
    }
    if (*m_type != CONTL_TYPE::PLAY)
    {
        int curFrame = _curPts / 1000.0 * m_cap.get(CAP_PROP_FPS);
        qDebug() << "curFrame:" << curFrame;
        m_cap.set(CAP_PROP_POS_FRAMES, curFrame);
        if (m_cap.read(m_frame))
        {
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            QPixmap frame = QPixmap::fromImage(QImage(m_frame.data, m_frame.cols, m_frame.rows, m_frame.step, QImage::Format_RGB888));
            emit VideoThread::sendFrame(_curPts, frame);
        }
    }
}