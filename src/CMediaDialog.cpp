#include "CMediaDialog.h"
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QStandardPaths>
#include <QStatusBar>

CMediaDialog::CMediaDialog(QWidget *parent) : QMainWindow(parent)
{
    this->resize(800, 600);

    openFile = new QAction("打开文件", this);
    connect(openFile, &QAction::triggered, this, &CMediaDialog::onOpenFile);
    this->menuBar()->addAction(openFile);

    // QAction *openRtsp = new QAction("打开RTSP", this);
    // this->menuBar()->addAction(openRtsp);
    // connect(openRtsp, &QAction::triggered, [this]() { //
    //     this->controlWidget->showVideo("rtsp://127.0.0.1:8554/0");
    // });

    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);

    centralWidget->setLayout(new QHBoxLayout(centralWidget));
    centralWidget->layout()->setContentsMargins(0, 0, 0, 0);

    QWidget *stackedWidget = new QWidget(this);
    centralWidget->layout()->addWidget(stackedWidget);

    listWidget = new QListWidget(centralWidget);
    centralWidget->layout()->addWidget(listWidget);
    listWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

    QStackedLayout *stackedLayout = new QStackedLayout(stackedWidget);
    stackedLayout->setContentsMargins(0, 0, 0, 0);
    stackedWidget->setLayout(stackedLayout);

    frameWidget = new FrameWidget(stackedWidget);
    stackedLayout->addWidget(frameWidget);

    controlWidget = new ControlWidget(stackedWidget);
    stackedLayout->addWidget(controlWidget);

    stackedLayout->setCurrentIndex(1);
    stackedLayout->setStackingMode(QStackedLayout::StackAll);
    connect(controlWidget->decodethPtr(), &Decoder::initVideoOutput, frameWidget, &FrameWidget::onInitVideoOutput);
    connect(controlWidget->decodethPtr()->getVideoDecoder(), &VideoDecoder::sendVideoFrame, frameWidget, &FrameWidget::receviceFrame);
    connect(controlWidget, &ControlWidget::openFileRequest, this, &CMediaDialog::onOpenFile);
    connect(controlWidget, &ControlWidget::fullScreenRequest, this, &CMediaDialog::onFullScreenRequest);
    connect(controlWidget, &ControlWidget::setListVisible, [this](bool visible)
            { listWidget->setVisible(visible); });
}

void CMediaDialog::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(this, "选择视频文件", QStandardPaths::writableLocation(QStandardPaths::StandardLocation::MoviesLocation), "Media Files(*.mp4 *.avi *.mkv *.mp3 *.wav);;All Files(*)");
    if (path.isEmpty())
        return;
    controlWidget->showVideo(path);
    this->statusBar()->showMessage(path);
}

void CMediaDialog::onOpenFolder()
{
    QString path = QFileDialog::getExistingDirectory(this, "选择视频文件夹", QStandardPaths::writableLocation(QStandardPaths::StandardLocation::MoviesLocation));
    if (path.isEmpty())
        return;
    listWidget->clear();
    QStringList filters = {"*.mp4", "*.mp3", "*.mkv", "*.avi", "*.wav"};
    QStringList files = QDir(path).entryList(filters, QDir::Files | QDir::Readable, QDir::Name);
    listWidget->addItems(files);
}

void CMediaDialog::onFullScreenRequest()
{
    // qDebug() << "onFullScreenRequest";
    if (this->isFullScreen())
    {
        // setWindowFlags(Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint | Qt::WindowFullscreenButtonHint);
        this->menuBar()->show();
        this->statusBar()->show();
        showNormal();
    }
    else
    { // // 窗口全屏化
        // setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        this->menuBar()->hide();
        this->statusBar()->hide();
        showFullScreen();
    }
}

ControlWidget::ControlWidget(QWidget *parent) : QWidget(parent)
{
    QApplication::instance()->installEventFilter(this);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);
    // 弹簧
    layout->addStretch();

    { // 进度条组(进度条+时间标签), 合并起来方便做动画 显示/隐藏
        sliderWidget = new QWidget(this);
        layout->addWidget(sliderWidget);
        sliderWidget->setLayout(new QHBoxLayout(sliderWidget));
        sliderWidget->layout()->setContentsMargins(0, 0, 0, 0);

        timeLabel = new QLabel("", sliderWidget);
        timeLabel->setStyleSheet("color: white;");
        sliderWidget->layout()->addWidget(timeLabel);

        slider = new CSlider(Qt::Horizontal, sliderWidget);
        // 滑块设置圆角
        slider->setStyleSheet({R"(
        background-color: transparent;

        QSlider::groove:horizontal {
            border: 1px solid #999999;
            height: 5px;
            background: #c4c4c4;
            /*margin: 2px 0;*/
        }
        QSlider::handle:horizontal {
            background: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0.6 #4CC2FF, stop:0.8 #5c5c5c);
            border: 2px solid #5c5c5c;
            width: 17px;
            height: 18px;
            margin: -8px 0; 
            border-radius: 10px;
        }
        QSlider::sub-page:horizontal {
            background: #4CC2FF; /* 滑块走过的部分颜色 */
        }
        QSlider::add-page:horizontal {
            background: #c4c4c4; /* 滑块未走过的部分颜色 */
        })"});
        sliderWidget->layout()->addWidget(slider);
        slider->setValue(0);
        slider->setRange(0, 0);

        totalTimeLabel = new QLabel("", sliderWidget);
        totalTimeLabel->setStyleSheet("color: white;");
        sliderWidget->layout()->addWidget(totalTimeLabel);

        listVisible = new QPushButton(sliderWidget);
        listVisible->setFixedSize(40, 30);
        listVisible->setCheckable(true);
        listVisible->setChecked(true);
        sliderWidget->layout()->addWidget(listVisible);
        connect(listVisible, &QPushButton::clicked, this, &ControlWidget::setListVisible);
    }

    decode_th = new Decoder(&m_type);
    decodeThread = new QThread();
    decode_th->moveToThread(decodeThread);
    decodeThread->start();
    connect(this, &ControlWidget::startPlay, decode_th, &Decoder::decodePacket);
    connect(decode_th, &Decoder::playOver, this, &ControlWidget::onPlayOver);

    audio_th = new AudioRenderer();
    audioThread = new QThread();
    audio_th->moveToThread(audioThread);
    audioThread->start();
    connect(decode_th, &Decoder::initAudioOutput, audio_th, &AudioRenderer::onInitAudioOutput, Qt::DirectConnection);
    connect(decode_th, &Decoder::getCurPts, audio_th, &AudioRenderer::onGetAudioClock, Qt::DirectConnection); // 必须直连
    connect(decode_th->getAudioDecoder(), &AudioDecoder::sendAudioBuffer, audio_th, &AudioRenderer::recvAudioBuffer);
    connect(audio_th, &AudioRenderer::audioClockChanged, this, &ControlWidget::onAudioClockChanged);

    // video_th = new VideoWaiter();
    // videoThread = new QThread();
    // video_th->moveToThread(videoThread);
    // videoThread->start();
    // connect(decode_th->getVideoDecoder(), &VideoDecoder::sendVideoFrame, video_th, &VideoWaiter::recvVideoFrame);
    // connect(video_th, &VideoWaiter::getAudioClock, audio_th, &AudioRenderer::onGetAudioClock, Qt::DirectConnection); // 必须直连

    connect(slider, &CSlider::sliderClicked, this, &ControlWidget::startSeek);
    connect(slider, &CSlider::sliderMoved, decode_th, &Decoder::setCurFrame);
    connect(slider, &CSlider::sliderReleased, this, &ControlWidget::endSeek);

    // connect(video_th, &VideoThread::finishPlay, this, &CMediaDialog::terminatePlay);
}

ControlWidget::~ControlWidget()
{
    terminatePlay();

    decode_th->deleteLater();
    audio_th->deleteLater();

    decodeThread->quit();
    decodeThread->wait();
    decodeThread->deleteLater();

    audioThread->quit();
    audioThread->wait();
    audioThread->deleteLater();

    // video_th->deleteLater();
    // videoThread->quit();
    // videoThread->wait();
    // videoThread->deleteLater();

    qDebug() << "ControlWidget::~ControlWidget()";
}

void ControlWidget::showVideo(const QString &path)
{
    if (m_type != CONTL_TYPE::NONE)
    {
        terminatePlay();
        slider->setValue(0);
        timeLabel->setText("00:00");
        totalTimeLabel->setText("00:00");
        qApp->processEvents(); // 强制更新UI
        QThread::msleep(100);
    }
    decode_th->setVideoPath(path);

    int64_t duration_ms = decode_th->getDuration();
    int duration_s = static_cast<int>(duration_ms / 1000);
    slider->setRange(0, duration_s);
    if (duration_s > 3600)
        totalTimeLabel->setText(QString::asprintf("%02d:%02d:%02d", duration_s / 3600, duration_s / 60 % 60, duration_s % 60));
    else
        totalTimeLabel->setText(QString::asprintf("%02d:%02d", duration_s / 60 % 60, duration_s % 60));
    m_type = CONTL_TYPE::PLAY;
    emit ControlWidget::startPlay();
}

void ControlWidget::resumeUI()
{
    slider->setValue(0);
    timeLabel->setText("00:00");
}

void ControlWidget::onAudioClockChanged(int pts_seconds)
{
    slider->setValue(pts_seconds);
    QString pts_str;
    if (pts_seconds > 3600)
        pts_str = QString::asprintf("%02d:%02d:%02d", pts_seconds / 3600, pts_seconds / 60 % 60, pts_seconds % 60);
    else
        pts_str = QString::asprintf("%02d:%02d", pts_seconds / 60 % 60, pts_seconds % 60);

    timeLabel->setText(pts_str);
}

void ControlWidget::changePlayState()
{
    switch (m_type)
    {
    case CONTL_TYPE::END:
        resumeUI();
        m_type = CONTL_TYPE::RESUME;
        break;

    case CONTL_TYPE::PLAY:
        m_type = CONTL_TYPE::PAUSE;
        break;

    case CONTL_TYPE::PAUSE:
        m_type = CONTL_TYPE::PLAY;
        break;

    case CONTL_TYPE::STOP:
    case CONTL_TYPE::RESUME:
        decode_th->resume();
        m_type = CONTL_TYPE::PLAY;
        break;

    case CONTL_TYPE::NONE:
        emit ControlWidget::openFileRequest();
        break;

    default:
        break;
    }
    emit ControlWidget::startPlay();
    debugPlayerCommand((CONTL_TYPE)m_type);
}

void ControlWidget::startSeek()
{
    m_type = (m_type == CONTL_TYPE::PLAY) ? CONTL_TYPE::SEEK_ON_PLAY : CONTL_TYPE::SEEK_ON_PAUSE;
}

void ControlWidget::endSeek()
{
    m_type = ((m_type == CONTL_TYPE::SEEK_ON_PLAY) ? CONTL_TYPE::PLAY : CONTL_TYPE::PAUSE);
    emit ControlWidget::startPlay();
}

void ControlWidget::terminatePlay()
{
    m_type = CONTL_TYPE::STOP;
}

void ControlWidget::onPlayOver()
{
    m_type = CONTL_TYPE::END;
    slider->setValue(slider->maximum());
    timeLabel->setText(totalTimeLabel->text());
}

void ControlWidget::mousePressEvent(QMouseEvent *event)
{
    // 如果鼠标的点在label内部
    if (event->pos().x() >= 0 && event->pos().x() <= this->width() && event->pos().y() >= 0 && event->pos().y() <= this->height())
    {
        if (event->button() == Qt::LeftButton)
            timerID = startTimer(250);
        else if (event->button() == Qt::RightButton)
            sliderWidget->setVisible(!sliderWidget->isVisible());
        // menu->exec(event->globalPos());
    }
}

void ControlWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->pos().x() >= 0 && event->pos().x() <= this->width() && event->pos().y() >= 0 && event->pos().y() <= this->height())
    {
        if (timerID)
            killTimer(timerID);
        timerID = 0;
        if (event->button() == Qt::LeftButton)
            emit fullScreenRequest();
    }
}

void ControlWidget::timerEvent(QTimerEvent *event)
{
    killTimer(event->timerId());
    timerID = 0;
    changePlayState();
}

void ControlWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Space:
        changePlayState();
        break;
    case Qt::Key_Left:
        slider->moveToValue(slider->value() - 10);
        break;
    case Qt::Key_Right:
        slider->moveToValue(slider->value() + 10);
        break;

    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

bool ControlWidget::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
        keyPressEvent(keyEvent);
        return true;
    }
    else
    {
        return QWidget::eventFilter(obj, e);
    }
}

#pragma region slider
void CSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->isPress = true;
        // 获取鼠标的位置，这里并不能直接从ev中取值（因为如果是拖动的话，鼠标开始点击的位置没有意义了）
        double pos = event->pos().x() / (double)width();
        setValue(pos * (maximum() - minimum()) + minimum());
        // qDebug() << "setValue: " << this->value();
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
        //     emit CSlider::sliderMoved(value);
        // }
    }
}

void CSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->isPress = false;
        emit CSlider::sliderMoved(this->value());
        // qDebug() << "slider_value: " << this->value();

        QSlider::mouseReleaseEvent(event);
        emit CSlider::sliderReleased();
    }
}

void CSlider::moveToValue(int value)
{
    emit sliderClicked();
    setValue(value);

    QThread::msleep(10); // 保证decoder线程暂停

    emit sliderMoved(value);
    emit sliderReleased();
}

void CSlider::setRange(int min, int max)
{
    one_percent = (max - min) / 100;
    QSlider::setRange(min, max);
}
#pragma endregion

void FrameWidget::timerEvent(QTimerEvent *event)
{
    killTimer(event->timerId());
    if (glWidget)
        glWidget->update();
}

FrameWidget::FrameWidget(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    this->setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    backgroundWidget = new QWidget(this);
    backgroundWidget->setStyleSheet("background-color: black;");
    layout->addWidget(backgroundWidget);
}

void FrameWidget::onInitVideoOutput(int format)
{
    if (backgroundWidget)
    {
        this->layout()->removeWidget(backgroundWidget);
        delete backgroundWidget;
        backgroundWidget = nullptr;
    }

    if (curGLWidgetFormat == format)
        return; // 避免重复创建

    curGLWidgetFormat = format;

    if (glWidget)
        delete glWidget;

    if (format == AV_HWDEVICE_TYPE_NONE)
    {
        qDebug() << "YUV420GL_WIDGET";
        glWidget = new Yuv420GLWidget(this);
        this->layout()->addWidget(glWidget);
    }
    else
    {
        qDebug() << "NV12GL_WIDGET";
        glWidget = new Nv12GLWidget(this);
        this->layout()->addWidget(glWidget);
    }
}

void FrameWidget::receviceFrame(uint8_t *pixelData, int pixelWidth, int pixelHeight, double picPts)
{
    if (glWidget)
    {
        glWidget->setPixelData(pixelData, pixelWidth, pixelHeight);

        double curPts = 0.0;
        getCurPts(curPts);
        int gap = qAbs(curPts - picPts);

        if (gap && curPts > 0.1)
            startTimer(curPts - picPts);
        else
            glWidget->update();
    }
    else if (pixelData)
        delete pixelData;
}
