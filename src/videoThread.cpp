#include "videoThread.h"

using namespace cv;
using namespace std;

VideoThread::VideoThread(int *_type, QTime *_time) : m_type(_type), m_time(_time)
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
    curFrame = 0;
    return m_cap.set(CAP_PROP_POS_FRAMES, curFrame);
}

bool VideoThread::setVideoPath(const QString &path)
{
    curFrame = 0;
    return m_cap.open(path.toStdString());
}

int VideoThread::getVideoFrameCount()
{
    int ret = m_cap.get(CAP_PROP_FRAME_COUNT);
    qDebug() << ret;
    return ret;
}

int VideoThread::getVideoDuration()
{
    int ret = m_cap.get(CAP_PROP_FRAME_COUNT) / m_cap.get(CAP_PROP_FPS);
    return ret;
}

void VideoThread::runPlay()
{
    if (*m_type == NONE)
        return;

    int last_ = -1000; // 保证第一帧无需等待
    while (*m_type == CONTL_TYPE::PLAY)
    {
        if (m_cap.read(m_frame))
        {
            curFrame++;
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            int sleepTime = getDiffTime() - getElapsed_AndUpdate(last_, m_time) - 1;
            if (sleepTime > 0)
            {
                // qDebug() << "video:" << sleepTime;
                QThread::msleep(sleepTime);
            }
            emit sendFrame(curFrame, m_frame);
        }
        else
        {
            emit finishPlay();
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

inline int VideoThread::getDiffTime()
{
    int cur_pts = m_cap.get(CAP_PROP_POS_MSEC);
    int ret = cur_pts - last_pts;
    last_pts = m_cap.get(CAP_PROP_POS_MSEC);
    return ret;
}
