#pragma once

#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QSlider>
#include <QMouseEvent>
#include "videoThread.h"
#include "audioThread.h"

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
    bool isPress{false};
    int lastLocation{0}; // 减少移动时发出切换帧信号次数
    int one_percent{0};

signals:
    void sliderClicked();
    void sliderMoved(int value);
    void sliderReleased();

protected:
    void mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            this->isPress = true;
            // 获取鼠标的位置，这里并不能直接从ev中取值（因为如果是拖动的话，鼠标开始点击的位置没有意义了）
            double pos = event->pos().x() / (double)width();
            setValue(pos * (maximum() - minimum()) + minimum());
            qDebug() << "setValue: " << this->value();
            emit VideoSlider::sliderClicked();
        }
    }

    void mouseMoveEvent(QMouseEvent *event)
    {
        // 如果鼠标左键被按下
        if (event->buttons() & Qt::LeftButton)
        {
            double pos = event->pos().x() / (double)width();
            int value = pos * (maximum() - minimum()) + minimum();
            setValue(value);

            if (qAbs(value - this->lastLocation) > (one_percent * 5))
            {
                lastLocation = value;
                emit VideoSlider::sliderMoved(value);
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            this->isPress = false;
            emit VideoSlider::sliderMoved(this->value());
            qDebug() << "slider_value: " << this->value();

            QSlider::mouseReleaseEvent(event);
            emit VideoSlider::sliderReleased();
        }
    }

public:
    VideoSlider(Qt::Orientation orientation, QWidget *parent = nullptr) : QSlider(orientation, parent) {}
    void setRange(int min, int max)
    {
        one_percent = (max - min) / 100;
        QSlider::setRange(min, max);
    }
    bool getIsPress() { return this->isPress; }
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

    // 强制关闭
    void terminatePlay();

private:
    CLabel *label;
    QPixmap pix;
    double ratio;
    VideoSlider *slider;
    VideoThread *video_th;
    AudioThread *audio_th;
    QTime *m_time;

    int m_type;

    bool isPlay = false; // 保存拖动进度条前视频播放状态

protected:
    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event);

public:
    CMediaDialog(QWidget *parent = nullptr);
    ~CMediaDialog();

    void showVideo(const QString &path);
    void changePlayState();

    int showPic(const QString &path);
};
