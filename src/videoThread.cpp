#include "videoThread.h"
#include <QDebug>

using namespace cv;

VideoThread::VideoThread()
{
    m_thread = new QThread;
    this->moveToThread(m_thread);
    connect(this, &VideoThread::startPlay, this, &VideoThread::runPlay);
    m_thread->start();
}

VideoThread::~VideoThread()
{
    m_thread->quit();
    m_thread->wait();
}

void VideoThread::setVideoPath(const QString &path)
{
    if (m_cap.open(path.toStdString()))
    {
        m_type = CONTL_TYPE::PLAY;
        curFrame = 0;
    }
}

int VideoThread::getVideoFrameCount()
{
    return m_cap.get(CAP_PROP_FRAME_COUNT);
}

void VideoThread::terminatePlay()
{
    m_type = CONTL_TYPE::END;
}

void VideoThread::runPlay()
{
    if (m_type == NONE)
        return;
    if (m_type == CONTL_TYPE::RESUME)
    {
        curFrame = 0;
        m_cap.set(CAP_PROP_POS_FRAMES, curFrame);
        m_type = CONTL_TYPE::PLAY;
    }
    while (m_type == CONTL_TYPE::PLAY)
    {

        if (m_cap.read(m_frame))
        {
            curFrame++;
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            emit sendFrame(curFrame, m_frame);
            qDebug() << curFrame;
        }
        else
        {
            m_type = CONTL_TYPE::END;
            break;
        }
        QThread::msleep(40); // 待修改
    }
}

void VideoThread::changePlayState()
{
    switch (m_type)
    {
    case CONTL_TYPE::END:
        m_type = CONTL_TYPE::RESUME;
        break;

    case CONTL_TYPE::PLAY:
        m_type = CONTL_TYPE::PAUSE;
        break;

    case CONTL_TYPE::PAUSE:
        m_type = CONTL_TYPE::PLAY;
        break;

    default:
        break;
    }
    emit VideoThread::startPlay();
}

void VideoThread::on_sliderPress()
{
    isPlay = (m_type == CONTL_TYPE::PLAY);
    m_type = CONTL_TYPE::PAUSE;
}

void VideoThread::on_sliderRelease()
{
    m_type = isPlay ? CONTL_TYPE::PLAY : CONTL_TYPE::PAUSE;
}

void VideoThread::setCurFrame(int _curFrame)
{
    curFrame = _curFrame;
    if (m_type != CONTL_TYPE::PLAY)
    {
        m_cap.set(CAP_PROP_POS_FRAMES, curFrame);
        if (m_cap.read(m_frame))
        {
            curFrame++;
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            emit VideoThread::sendFrame(curFrame, m_frame);
        }
        else
        {
            m_cap.set(CAP_PROP_POS_FRAMES, --curFrame); // 后退一帧
        }
    }
}
