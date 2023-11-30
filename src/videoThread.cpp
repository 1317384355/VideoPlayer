#include "videoThread.h"
#include "demo.h"
#include <QDebug>
#include <QWaitCondition>

#define capacity 100

using namespace cv;
using namespace std;

VideoThread::VideoThread(queue<CFrame *> *_queue, int *_type) : m_queue(_queue), m_type(_type)
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
    Mat frame;
    while (*m_type == CONTL_TYPE::PLAY)
    {
        if (m_cap.read(frame))
        {
            curFrame++;
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);

            m_queue->push(new CFrame(m_frame, m_cap.get(CAP_PROP_POS_MSEC)));
        }
        else
        {
            *m_type = CONTL_TYPE::END;
            break;
        }

        if (m_queue->size() > capacity)
        {
            QWaitCondition wait;
            wait.wait(&m_mutex, 1000);
        }
    }
}

inline Mat VideoThread::getNextFrame()
{
    Mat frame;
    if (m_cap.read(frame))
    {
        curFrame++;
        cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
    }
    return frame;
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
