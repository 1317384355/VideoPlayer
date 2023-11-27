#pragma once
#include <QThread>

class VideoThread : public QObject
{
    Q_OBJECT
private:
public:
    VideoThread();
    ~VideoThread();
};
