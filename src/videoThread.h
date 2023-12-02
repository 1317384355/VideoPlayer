#pragma once
#include "audioThread.h"
#include <opencv2/opencv.hpp>

class VideoThread : public QObject
{
    Q_OBJECT
signals:
    // 发送帧数据(当前帧进度, 当前帧)
    void sendFrame(int curPts, cv::Mat frame);
    // 开始播放信号, 必须由此信号发出的播放才可在子线程中播放
    void startPlay();
    void finishPlay();

private slots:
    // 播放\重播 视频
    void runPlay();

public slots:
    // 响应拖动进度条, 跳转到帧并返回这一帧画面
    void setCurFrame(int _curPts);

private:
    const AudioThread *m_audio; // 仅用于获取时钟
    const int *m_type;          // 同步播放状态
    QThread *m_thread;          // 解码视频线程
    cv::VideoCapture m_cap;     // 解码视频对象实例
    cv::Mat m_frame;            // 暂存当前帧画面, 转换格式, 然后发送至播放窗口
    int curPts = 0;             // 当前时间戳(单位ms)

public:
    VideoThread(const int *_type, const AudioThread *_audio);
    ~VideoThread();

    bool resume();

    // 设置视频路径
    bool setVideoPath(const QString &path);

    // 得到当前视频总帧数
    double getVideoFrameCount();

    double getVideoDuration();
};
