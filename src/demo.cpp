#include "demo.h"
#include "audioThread.h"
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QDebug>

CONTL_TYPE m_type = CONTL_TYPE::NONE;

using namespace cv;

CMediaDialog::CMediaDialog(QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle("图片预览");
    this->resize(300, 300);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    layout->addWidget(scrollArea);

    QWidget *contenter = new QWidget(scrollArea);
    scrollArea->setWidget(contenter);

    QVBoxLayout *m_layout = new QVBoxLayout(contenter);
    scrollArea->setLayout(m_layout);
    m_layout->setContentsMargins(0, 0, 0, 0);

    label = new CLabel(this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("QLabel{background-color: rgba(0, 0, 0, 255);}");
    m_layout->addWidget(label);

    slider = new VideoSlider(Qt::Horizontal, this);
    slider->hide();
    m_layout->addWidget(slider);

    this->showVideo("./qrc/1.mp4");
}

CMediaDialog::~CMediaDialog()
{
    terminatePlay();
}

void CMediaDialog::receviceFrame(int curFrame, cv::Mat frame)
{
    if (slider->getIsMove() == false)
    {
        slider->setValue(curFrame);
    }
    pix = QPixmap::fromImage(QImage(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888));
    ratio = (double)pix.width() / pix.height();
    update();
}

void CMediaDialog::showVideo(const QString &path)
{

    AudioThread *thread = new AudioThread;
    thread->setAudioPath(path);
    thread->getAudioFrameCount();

    slider->show();

    video_th = new VideoThread;
    video_th->setVideoPath(path);
    slider->setRange(0, video_th->getVideoFrameCount());
    slider->setValue(0);

    connect(video_th, &VideoThread::sendFrame, this, &CMediaDialog::receviceFrame, Qt::DirectConnection); //  Qt::DirectConnection 必须

    connect(label, &CLabel::clicked, this, &CMediaDialog::changePlayState);

    connect(slider, &VideoSlider::sliderClicked, this, &CMediaDialog::startSeek);
    connect(slider, &VideoSlider::sliderMoved, video_th, &VideoThread::setCurFrame);
    connect(slider, &VideoSlider::sliderReleased, this, &CMediaDialog::endSeek);

    connect(this, &CMediaDialog::startPlay, video_th, &VideoThread::startPlay);
    connect(this, &CMediaDialog::startPlay, thread, &AudioThread::startPlay);

    emit this->startPlay();
    this->exec();
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

int CMediaDialog::showPic(const QString &path)
{
    Mat img = imread(path.toStdString());
    if (img.empty())
    {
        QMessageBox::warning(this, "警告", "图片不存在");
        return QDialog::Rejected;
    }
    cvtColor(img, img, COLOR_BGR2RGB);

    receviceFrame(0, img);
    return this->exec();
}

void CMediaDialog::resizeEvent(QResizeEvent *event)
{
    double widgetRatio = label->width() / 1.0 / label->height();
    if (widgetRatio > ratio)
    {
        label->setPixmap(pix.scaled((label->height()) * ratio, (label->height())));
    }
    else
    {
        label->setPixmap(pix.scaled((label->width()), (label->width()) / ratio));
    }
}

void CMediaDialog::paintEvent(QPaintEvent *event)
{
    double widgetRatio = label->width() / 1.0 / label->height();
    if (widgetRatio > ratio)
    {
        label->setPixmap(pix.scaled((label->height()) * ratio, (label->height())));
    }
    else
    {
        label->setPixmap(pix.scaled((label->width()), (label->width()) / ratio));
    }
}
