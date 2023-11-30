#pragma once

#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QSlider>
#include <QMouseEvent>
#include "videoThread.h"
#include <opencv2/opencv.hpp>

enum CONTL_TYPE
{
    NONE,
    PLAY,
    PAUSE,
    RESUME,
    END,
};

extern CONTL_TYPE m_type;

class CLabel : public QLabel
{
    Q_OBJECT
public:
    explicit CLabel(QWidget *parent = nullptr) : QLabel(parent) {}

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        // 如果鼠标的点在label内部
        if (event->pos().x() >= 0 && event->pos().x() <= this->width() &&
            event->pos().y() >= 0 && event->pos().y() <= this->height())
        {
            emit CLabel::clicked();
        }
    }
};

class VideoSlider : public QSlider
{
    Q_OBJECT
private:
    bool isMove = false;
    int moveCount = 0; // 减少移动时发出切换帧信号次数

signals:
    void sliderClicked();
    void sliderMoved(int value);
    void sliderReleased();

protected:
    void mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            // 获取鼠标的位置，这里并不能直接从ev中取值（因为如果是拖动的话，鼠标开始点击的位置没有意义了）
            double pos = event->pos().x() / (double)width();
            setValue(pos * (maximum() - minimum()) + minimum());
            emit VideoSlider::sliderClicked();
        }
    }

    void mouseMoveEvent(QMouseEvent *event)
    {
        // 如果鼠标左键被按下
        if (event->buttons() & Qt::LeftButton)
        {
            this->isMove = true;
            double pos = event->pos().x() / (double)width();
            setValue(pos * (maximum() - minimum()) + minimum());
            moveCount++;
            if (moveCount > 15)
            {
                moveCount = 0;
                emit VideoSlider::sliderMoved(this->value());
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            this->isMove = false;
            QSlider::mouseReleaseEvent(event);
            emit VideoSlider::sliderMoved(this->value());
            emit VideoSlider::sliderReleased();
        }
    }

public:
    VideoSlider(Qt::Orientation orientation, QWidget *parent = nullptr) : QSlider(orientation, parent) {}

    bool getIsMove() { return this->isMove; }
};

class CMediaDialog : public QDialog
{
    Q_OBJECT
signals:
    void startPlay();

private slots:
    void receviceFrame(int curFrame, cv::Mat frame);

    // 响应拖动进度条, 当鼠标压下时暂停, 并保存播放状态
    void startSeek();

    // 响应拖动进度条, 当鼠标松开时恢复播放状态
    void endSeek();

private:
    CLabel *label;
    QPixmap pix;
    double ratio;
    VideoSlider *slider;
    VideoThread *video_th;

    bool isPlay = false; // 保存拖动进度条前视频播放状态

protected:
    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event);

public:
    CMediaDialog(QWidget *parent = nullptr);
    ~CMediaDialog();

    void showVideo(const QString &path);
    void changePlayState();
    // 强制关闭
    void terminatePlay();

    int showPic(const QString &path);
};
