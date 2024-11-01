#include "demo.h"
#include "audioThread.h"
#include <QMessageBox>
#include <QPaintEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

using namespace cv;

demo::demo(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);

    // 选择视频并播放功能
    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton *btnSelect = new QPushButton("选择", this);
    QLineEdit *lineEdit = new QLineEdit(this);
    lineEdit->setReadOnly(true); // 设置只读
    hLayout->addWidget(btnSelect);
    hLayout->addWidget(lineEdit);
    layout->addLayout(hLayout);

    CMediaDialog *w = new CMediaDialog(this);
    layout->addWidget(w);
    this->resize(700, 400);

    // 连接槽, 选择视频并播放
    connect(btnSelect, &QPushButton::clicked, [=]() { //
        QString path = QFileDialog::getOpenFileName(this, "选择视频文件", "", "Video Files(*.mp4 *.avi *.mkv)");
        if (!path.isEmpty())
        {
            // 仅显示文件名, 并且去掉后缀
            lineEdit->setText(path.mid(path.lastIndexOf("/") + 1, path.lastIndexOf(".") - path.lastIndexOf("/") - 1));
            w->showVideo(path);
        }
    });
}

demo::~demo()
{
}

CMediaDialog::CMediaDialog(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);

    frameWidget = new FrameWidget(this);
    layout->addWidget(frameWidget);

    slider = new VideoSlider(Qt::Horizontal, this);
    layout->addWidget(slider);
}

CMediaDialog::~CMediaDialog()
{
    terminatePlay();
    this->disconnect();
}

void CMediaDialog::receviceFrame(int curMs, const QPixmap &frame)
{
    if (slider->getIsPress() == false)
    {
        slider->setValue(curMs);
    }
    frameWidget->setPixmap(frame);
}

void CMediaDialog::showVideo(const QString &path)
{
    this->show();
    slider->setValue(0);

    audio_th = new AudioThread(&m_type);
    audio_th->setAudioPath(path);

    video_th = new VideoThread(&m_type, audio_th);
    video_th->setVideoPath(path);
    video_th->getVideoDuration();
    slider->setRange(0, audio_th->getAudioDuration());

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

    connect(video_th, &VideoThread::sendFrame, this, &CMediaDialog::receviceFrame, Qt::DirectConnection); //  Qt::DirectConnection 必须

    connect(frameWidget, &FrameWidget::clicked, this, &CMediaDialog::changePlayState);

    connect(slider, &VideoSlider::sliderClicked, this, &CMediaDialog::startSeek);
    connect(slider, &VideoSlider::sliderMoved, video_th, &VideoThread::setCurFrame);
    connect(slider, &VideoSlider::sliderMoved, audio_th, &AudioThread::setCurFrame);
    connect(slider, &VideoSlider::sliderReleased, this, &CMediaDialog::endSeek);

    connect(this, &CMediaDialog::startPlay, video_th, &VideoThread::startPlay);
    connect(this, &CMediaDialog::startPlay, audio_th, &AudioThread::startPlay);
    connect(video_th, &VideoThread::finishPlay, this, &CMediaDialog::terminatePlay);

    m_type = CONTL_TYPE::PLAY;
    emit this->startPlay();
}

void CMediaDialog::changePlayState()
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
        video_th->resume();
        audio_th->resume();
        m_type = CONTL_TYPE::PLAY;
        break;

    default:
        break;
    }
    emit CMediaDialog::startPlay();
}

void CMediaDialog::startSeek()
{
    isPlay = (m_type == CONTL_TYPE::PLAY);
    m_type = CONTL_TYPE::PAUSE;
}

void CMediaDialog::endSeek()
{
    m_type = (isPlay ? CONTL_TYPE::PLAY : CONTL_TYPE::PAUSE);
    emit CMediaDialog::startPlay();
}

void CMediaDialog::terminatePlay()
{
    m_type = CONTL_TYPE::END;
}
