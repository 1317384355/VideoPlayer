#include "videoThread.h"

using namespace cv;
using namespace std;

VideoThread::VideoThread(const int *_type, const AudioThread *_audio) : m_type(_type), m_audio(_audio)
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
    double ret = m_cap.get(CAP_PROP_FRAME_COUNT) / m_cap.get(CAP_PROP_FPS) * 1000;
    qDebug() << m_cap.get(CAP_PROP_FRAME_COUNT) << m_cap.get(CAP_PROP_FPS) << ret;
    return ret;
}

void VideoThread::runPlay()
{
    if (*m_type == NONE)
        return;

    while (*m_type == CONTL_TYPE::PLAY)
    {
        if (m_cap.read(m_frame))
        {
            curPts = m_cap.get(CAP_PROP_POS_MSEC);
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            int sleepTime = curPts - m_audio->getAudioClock();
            if (sleepTime > 0)
            {
                // qDebug() << "video:" << sleepTime;
                QThread::msleep(sleepTime);
            }
            emit sendFrame(curPts, m_frame);
        }
        else
        {
            emit finishPlay();
            break;
        }
    }
}

void VideoThread::setCurFrame(int _curPts)
{
    int curFrame = _curPts / 1000.0 * m_cap.get(CAP_PROP_FPS);
    qDebug() << "curFrame:" << curFrame;
    if (*m_type != CONTL_TYPE::PLAY)
    {
        m_cap.set(CAP_PROP_POS_FRAMES, curFrame);
        if (m_cap.read(m_frame))
        {
            cvtColor(m_frame, m_frame, COLOR_BGR2RGB);
            emit VideoThread::sendFrame(_curPts, m_frame);
        }
    }
}