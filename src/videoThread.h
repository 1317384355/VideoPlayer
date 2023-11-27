#pragma once
#include <opencv2/opencv.hpp>
#include <QThread>

class VideoThread : public QObject
{
    Q_OBJECT
signals:
    void sendFrame(int curFrame, cv::Mat frame);
    void startPlay();

public slots:
    void runPlay();
    void setCurFrame(int _curFrame);

private:
    enum CONTL_TYPE
    {
        NONE,
        PLAY,
        PAUSE,
        RESUME,
        END,
    };
    QThread *m_thread;
    CONTL_TYPE m_type = CONTL_TYPE::NONE;
    cv::VideoCapture m_cap;
    cv::Mat m_frame;
    int curFrame = 0;

    bool isPlay = false;

public:
    VideoThread();
    ~VideoThread();

    void setVideoPath(const QString &path);
    int getVideoFrameCount();

    void terminatePlay();

    void changePlayState();

    void on_sliderPress();
    void on_sliderRelease();
};
