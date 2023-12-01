#pragma once
#include "playerCommand.h"
#include <opencv2/opencv.hpp>

struct CFrame
{
    cv::Mat frame;
    double pts;
    CFrame(const cv::Mat &frame, double pts) : frame(frame), pts(pts) {}
};

class VideoThread : public QObject
{
    Q_OBJECT
signals:
    // 发送帧数据(当前帧进度, 当前帧)
    void sendFrame(int curFrame, cv::Mat frame);
    // 开始播放信号, 必须由此信号发出的播放才可在子线程中播放
    void startPlay();
    void finishPlay();

private slots:
    // 播放\重播 视频
    void runPlay();

public slots:
    // 响应拖动进度条, 跳转到帧并返回这一帧画面
    void setCurFrame(int _curFrame);

private:
    QTime *m_time;
    int *m_type;
    QThread *m_thread;      // 解码视频线程
    cv::VideoCapture m_cap; // 解码视频对象实例
    cv::Mat m_frame;        // 暂存当前帧画面, 转换格式, 然后发送至播放窗口
    int curFrame = 0;       // 当前帧进度
    int last_pts = 0;

public:
    VideoThread(int *_type, QTime *_time);
    ~VideoThread();

    bool resume();

    // 设置视频路径
    bool setVideoPath(const QString &path);

    inline int getDiffTime();

    // 得到当前视频总帧数
    int getVideoFrameCount();

    int getVideoDuration();
};
