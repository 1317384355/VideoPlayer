#include "videoThread.h"
#include "demo.h"
#include <QDebug>
#include <QWaitCondition>

#define capacity 100

using namespace cv;
using namespace std;

VideoThread::VideoThread(int *_type, QElapsedTimer *_time) : m_type(_type), m_time(_time)
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

void VideoThread::setVideoPath(const QString &path)
{
    if (m_cap.open(path.toStdString()))
    {
        *m_type = CONTL_TYPE::PLAY;
        curFrame = 0;
    }
}

qint64 VideoThread::getVideoFrameCount()
{
    qint64 ret = m_cap.get(CAP_PROP_FRAME_COUNT);
    qDebug() << ret;
    return ret;
}

void VideoThread::runPlay()
{
    if (*m_type == NONE)
        return;
    if (*m_type == CONTL_TYPE::RESUME)
    {
        curFrame = 0;
        m_cap.set(CAP_PROP_POS_FRAMES, curFrame);
        *m_type = CONTL_TYPE::PLAY;
    }

    while (*m_type == CONTL_TYPE::PLAY)
    {
        if (m_cap.read(m_frame))
        {
            curFrame++;
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            int sleepTime = m_cap.get(CAP_PROP_POS_MSEC) - m_time->elapsed() - 1;
            if (sleepTime > 0)
            {
                QThread::msleep(sleepTime);
            }
            emit sendFrame(curFrame, m_frame);
        }
        else
        {
            // *m_type = CONTL_TYPE::END;
            // emit finishPlay();
            break;
        }
    }
}

void VideoThread::setCurFrame(int _curFrame)
{
    curFrame = _curFrame;
    if (*m_type != CONTL_TYPE::PLAY)
    {
        m_cap.set(CAP_PROP_POS_FRAMES, curFrame);
        if (m_cap.read(m_frame))
        {
            curFrame++;
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            emit VideoThread::sendFrame(curFrame, m_frame);
        }
    }
}
