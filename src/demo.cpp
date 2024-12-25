#include "demo.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

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
        // QString test_path = "E:/Anime/GIRLS BAND CRY/[Nekomoe kissaten][GIRLS BAND CRY][01][1080p][JPSC].mp4";
        // if (QFileInfo(test_path).isFile())
        // {
        //     w->showVideo(test_path);
        //     return;
        // }

        QString path = QFileDialog::getOpenFileName(this, "选择视频文件", "", "Media Files(*.mp4 *.avi *.mkv *.mp3 *.wav);;All Files(*)");
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
