#include "CMediaDialog.h"
#include "AudioThread.h"
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QStackedLayout>

CMediaDialog::CMediaDialog(QWidget *parent) : QWidget(parent)
{
    this->setStyleSheet("background-color: rgb(0, 0, 0);");
    QStackedLayout *stackedLayout = new QStackedLayout(this);
    stackedLayout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(stackedLayout);

    frameWidget = new FrameWidget(this);
    // frameWidget->setStyleSheet("background-color: rgb(255, 0, 0);");
    stackedLayout->addWidget(frameWidget);

    controlWidget = new ControlWidget(this);
    stackedLayout->addWidget(controlWidget);

    stackedLayout->setCurrentIndex(1);
    stackedLayout->setStackingMode(QStackedLayout::StackAll);
    connect(controlWidget->decodethPtr(), &Decode::initVideoOutput, frameWidget, &FrameWidget::onInitVideoOutput);
    connect(controlWidget->videothPtr(), &VideoThread::sendFrame, frameWidget, &FrameWidget::receviceFrame);
    connect(controlWidget, ControlWidget::fullScreenRequest, [=]() { //
        // qDebug() << "fullScreenRequest";
        if (this->isFullScreen())
        {
            if (this->parentWidget())
                this->parentWidget()->show();
            setWindowFlags(Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint | Qt::WindowFullscreenButtonHint);
            showNormal();
        }
        else
        { // // 窗口全屏化
            if (this->parentWidget())
                this->parentWidget()->hide();
            setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
            showFullScreen();
        }
    });
}

ControlWidget::ControlWidget(QWidget *parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);
    // 弹簧
    layout->addStretch();

    { // 进度条组(进度条+时间标签), 合并起来方便做动画 显示/隐藏
        sliderWidget = new QWidget(this);
        sliderWidget->setStyleSheet("background-color: transparent;");
        layout->addWidget(sliderWidget);
        sliderWidget->setLayout(new QHBoxLayout(sliderWidget));
        sliderWidget->layout()->setContentsMargins(0, 0, 0, 0);

        slider = new CSlider(Qt::Horizontal, this);
        sliderWidget->layout()->addWidget(slider);
        slider->setValue(0);
        slider->setRange(0, 0);

        timeLabel = new QLabel("00:00:00", this);
        timeLabel->setStyleSheet("color: rgb(255, 255, 255);");
        sliderWidget->layout()->addWidget(timeLabel);
    }

    decode_th = new Decode(&m_type);
    decodeThread = new QThread();
    decode_th->moveToThread(decodeThread);
    decodeThread->start();
    connect(this, &ControlWidget::startPlay, decode_th, &Decode::decodePacket);

    audio_th = new AudioThread();
    audioThread = new QThread();
    audio_th->moveToThread(audioThread);
    audioThread->start();
    connect(decode_th, &Decode::initAudioOutput, audio_th, &AudioThread::onInitAudioOutput);
    connect(decode_th, &Decode::sendAudioData, audio_th, &AudioThread::recvAudioData);
    connect(audio_th, &AudioThread::audioOutputReady, this, &ControlWidget::startPlay);
    connect(audio_th, &AudioThread::audioClockChanged, this, &ControlWidget::onAudioClockChanged);
    connect(audio_th, &AudioThread::audioDataUsed, decode_th, &Decode::onAudioDataUsed, Qt::DirectConnection); // 必须直连

    video_th = new VideoThread();
    videoThread = new QThread();
    video_th->moveToThread(videoThread);
    videoThread->start();
    connect(decode_th, &Decode::sendVideoData, video_th, &VideoThread::recvVideoData);
    connect(video_th, &VideoThread::videoDataUsed, decode_th, &Decode::onVideoDataUsed, Qt::DirectConnection);     // 必须直连
    connect(video_th, &VideoThread::getAudioClock, audio_th, &AudioThread::onGetAudioClock, Qt::DirectConnection); // 必须直连

    // this->label->menu = new QMenu(this);
    // auto actSS = new QAction("开始保存", this->label->menu);
    // this->label->menu->addAction(actSS);
    // auto actES = new QAction("结束保存", this->label->menu);
    // this->label->menu->addAction(actES);
    // connect(actSS, &QAction::triggered, [=]() { //
    //     video_th->startSave();
    // });
    // connect(actES, &QAction::triggered, [=]() { //
    //     video_th->endSava();
    // });

    connect(slider, &CSlider::sliderClicked, this, &ControlWidget::startSeek);
    // connect(slider, &VideoSlider::sliderMoved, video_th, &VideoThread::setCurFrame);
    // connect(slider, &VideoSlider::sliderMoved, audio_th, &AudioThread::setCurFrame);
    connect(slider, &CSlider::sliderReleased, this, &ControlWidget::endSeek);

    // connect(this, &CMediaDialog::startPlay, video_th, &VideoThread::startPlay);
    // connect(this, &CMediaDialog::startPlay, audio_th, &AudioThread::startPlay);
    // connect(video_th, &VideoThread::finishPlay, this, &CMediaDialog::terminatePlay);
}

ControlWidget::~ControlWidget()
{
    terminatePlay();
    this->disconnect();
}

void ControlWidget::showVideo(const QString &path)
{
    if (m_type != CONTL_TYPE::NONE)
    {
        terminatePlay();
        QThread::msleep(100);
    }
    decode_th->setVideoPath(path);
    int64_t duration_ms = decode_th->getDuration();
    int duration_s = static_cast<int>(duration_ms / 1000);
    slider->setRange(0, duration_s);
    timeLabel->setText(QString::asprintf("%02d:%02d:%02d", duration_s / 3600, duration_s / 60 % 60, duration_s % 60));
    m_type = CONTL_TYPE::PLAY;
}
void ControlWidget::onAudioClockChanged(int pts_seconds, QString pts_str)
{
    slider->setValue(pts_seconds);
    timeLabel->setText(pts_str);
}

void ControlWidget::changePlayState()
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

    case CONTL_TYPE::RESUME:
        // video_th->resume();
        // audio_th->resume();
        m_type = CONTL_TYPE::PLAY;
        break;

    default:
        break;
    }
    emit ControlWidget::startPlay();
}

void ControlWidget::startSeek()
{
    isPlay = (m_type == CONTL_TYPE::PLAY);
    m_type = CONTL_TYPE::PAUSE;
}

void ControlWidget::endSeek()
{
    m_type = (isPlay ? CONTL_TYPE::PLAY : CONTL_TYPE::PAUSE);
    emit ControlWidget::startPlay();
}

void ControlWidget::terminatePlay()
{
    m_type = CONTL_TYPE::END;
}

void ControlWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // 如果鼠标的点在label内部
    if (event->pos().x() >= 0 && event->pos().x() <= this->width() && event->pos().y() >= 0 && event->pos().y() <= this->height())
    {
        if (event->button() == Qt::LeftButton)
            changePlayState();
        else if (event->button() == Qt::RightButton)
            sliderWidget->setVisible(!sliderWidget->isVisible());
        // menu->exec(event->globalPos());
    }
}

void ControlWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->pos().x() >= 0 && event->pos().x() <= this->width() && event->pos().y() >= 0 && event->pos().y() <= this->height())
    {
        if (event->button() == Qt::LeftButton)
            emit fullScreenRequest();
        // else if (event->button() == Qt::RightButton)
        //     sliderWidget->setVisible(!sliderWidget->isVisible());
    }
}

void CSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->isPress = true;
        // 获取鼠标的位置，这里并不能直接从ev中取值（因为如果是拖动的话，鼠标开始点击的位置没有意义了）
        double pos = event->pos().x() / (double)width();
        setValue(pos * (maximum() - minimum()) + minimum());
        qDebug() << "setValue: " << this->value();
        emit CSlider::sliderClicked();
    }
}

void CSlider::mouseMoveEvent(QMouseEvent *event)
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

void CSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->isPress = false;
        emit CSlider::sliderMoved(this->value());
        qDebug() << "slider_value: " << this->value();

        QSlider::mouseReleaseEvent(event);
        emit CSlider::sliderReleased();
    }
}

void CSlider::setRange(int min, int max)
{
    one_percent = (max - min) / 100;
    QSlider::setRange(min, max);
}

FrameWidget::FrameWidget(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    this->setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
}

void FrameWidget::onInitVideoOutput(int format)
{
    if (curGLWidgetFormat == format)
        return; // 避免重复创建

    curGLWidgetFormat = format;

    if (glWidget)
        delete glWidget;

    switch (format)
    {
    case AV_PIX_FMT_NONE:
        qDebug() << "YUV420GL_WIDGET";
        glWidget = new Yuv420GLWidget(this);
        this->layout()->addWidget(glWidget);
        break;

    case AV_PIX_FMT_CUDA:
        qDebug() << "NV12GL_WIDGET";
        glWidget = new Nv12GLWidget(this);
        this->layout()->addWidget(glWidget);
        break;
    default:

        break;
    }
}

void FrameWidget::receviceFrame(uint8_t *pixelData, int pixelWidth, int pixelHeight)
{
    if (glWidget)
        glWidget->setPixelData(pixelData, pixelWidth, pixelHeight);
}