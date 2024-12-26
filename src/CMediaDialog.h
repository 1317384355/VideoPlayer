#pragma once

#include "AudioThread.h"
#include "Decode.h"
#include "OpenGLWidget.h"
#include "VideoThread.h"
#include "playerCommand.h"
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

// 画面窗口
class FrameWidget : public QWidget
{
    Q_OBJECT
public slots:
    void onInitVideoOutput(int format);
    void receviceFrame(uint8_t *pixelData, int pixelWidth, int pixelHeight);

private:
    int curGLWidgetFormat{-1};
    BaseOpenGLWidget *glWidget = nullptr; // OpenGL窗口
    QWidget *backgroundWidget = nullptr;  // 背景窗口

public:
    explicit FrameWidget(QWidget *parent = nullptr);
    ~FrameWidget() {}

    // const COpenGLWidget *glWidgetPtr() { return glWidget; }
};

// 视频进度条
class CSlider : public QSlider
{
    Q_OBJECT
private:
    bool isPress{false};
    int lastLocation{0}; // 减少移动时发出切换帧信号次数
    int one_percent{0};

signals:
    void sliderClicked();
    void sliderMoved(int value);
    void sliderReleased();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

public:
    CSlider(Qt::Orientation orientation, QWidget *parent = nullptr) : QSlider(orientation, parent) {}

    void setRange(int min, int max);
    bool getIsPress() const { return this->isPress; }
};

class ControlWidget : public QWidget
{
    Q_OBJECT

signals:
    void startPlay();
    void leftClicked();
    void rightClicked();
    // 全屏请求
    void fullScreenRequest();

private slots:
    // 响应音频进度条
    void onAudioClockChanged(int pts_seconds);

    // 响应拖动进度条, 当鼠标压下时暂停, 并保存播放状态
    void startSeek();

    // 响应拖动进度条, 当鼠标松开时恢复播放状态
    void endSeek();

    // 强制关闭
    void terminatePlay();

private:
    QWidget *sliderWidget{nullptr};
    CSlider *slider{nullptr};
    QLabel *timeLabel{nullptr};
    QLabel *totalTimeLabel{nullptr};
    QPushButton *btn{nullptr}; // 测试用
    QMenu *menu{nullptr};

    Decode *decode_th{nullptr};
    VideoThread *video_th{nullptr};
    AudioThread *audio_th{nullptr};
    QThread *decodeThread{nullptr};
    QThread *videoThread{nullptr};
    QThread *audioThread{nullptr};

    int m_type{NONE};

    bool isPlay = false; // 保存拖动进度条前视频播放状态

protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    // virtual void mouseDoubleClickEvent(QMouseEvent *event) override;

public:
    ControlWidget(QWidget *parent = nullptr);
    ~ControlWidget();

    const Decode *decodethPtr() { return decode_th; }
    const VideoThread *videothPtr() { return video_th; }
    void showVideo(const QString &path);
    void changePlayState();
};

class CMediaDialog : public QWidget
{
    Q_OBJECT

public slots:
    void onFullScreenRequest();

private:
    ControlWidget *controlWidget{nullptr};
    FrameWidget *frameWidget{nullptr};

public:
    CMediaDialog(QWidget *parent = nullptr);
    ~CMediaDialog() {}

    void showVideo(const QString &path) { controlWidget->showVideo(path); }
};
