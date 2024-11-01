#pragma once

#include "audioThread.h"
#include "videoThread.h"
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSlider>
#include <QWidget>
#include <QApplication>

class demo : public QWidget
{
    Q_OBJECT
private:
public:
    demo(QWidget *parent = nullptr);
    ~demo();
};

// 画面窗口
class FrameWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FrameWidget(QWidget *parent = nullptr) : QWidget(parent) {}
    ~FrameWidget() {}

    void setPixmap(const QPixmap &pix)
    {
        // 这里不复制, 屏幕会不刷新
        this->pix = pix.scaled(this->rect().size(), Qt::AspectRatioMode::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation);
        update();
    }

private:
    QMenu *menu;
    QPixmap pix;

signals:
    void clicked();

protected:
    virtual void paintEvent(QPaintEvent *event) override
    {
        if (!pix.isNull())
        {
            // 绘制图像
            QPainter painter(this);
            auto rect = event->rect();
            QPixmap pixmap = rect == this->pix.rect() ? this->pix : this->pix.scaled(rect.size(), Qt::AspectRatioMode::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation);
            // 画面居中
            painter.drawPixmap(rect.x() + (rect.width() - pixmap.width()) / 2, rect.y() + (rect.height() - pixmap.height()) / 2, pixmap);
        }
        QWidget::paintEvent(event);
    }

    virtual void mouseReleaseEvent(QMouseEvent *event) override
    {
        // 如果鼠标的点在label内部
        if (event->pos().x() >= 0 && event->pos().x() <= this->width() && event->pos().y() >= 0 && event->pos().y() <= this->height())
        {
            if (event->button() == Qt::LeftButton)
                emit this->clicked();
            else if (event->button() == Qt::RightButton)
                menu->exec(event->globalPos());
        }
    }
};

// 视频进度条
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

            // if (qAbs(value - this->lastLocation) > (one_percent * 5))
            // {
            //     lastLocation = value;
            //     emit VideoSlider::sliderMoved(value);
            // }
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

class CMediaDialog : public QWidget
{
    Q_OBJECT
signals:
    void startPlay();

private slots:
    void receviceFrame(int curFrame, const QPixmap &frame);

    // 响应拖动进度条, 当鼠标压下时暂停, 并保存播放状态
    void startSeek();

    // 响应拖动进度条, 当鼠标松开时恢复播放状态
    void endSeek();

    // 强制关闭
    void terminatePlay();

private:
    FrameWidget *frameWidget{nullptr};
    VideoSlider *slider{nullptr};

    VideoThread *video_th{nullptr};
    AudioThread *audio_th{nullptr};
    QTime *m_time{nullptr};

    int m_type;

    bool isPlay = false; // 保存拖动进度条前视频播放状态

public:
    CMediaDialog(QWidget *parent = nullptr);
    ~CMediaDialog();

    void showVideo(const QString &path);
    void changePlayState();
};
