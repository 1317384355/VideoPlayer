#pragma once
#include <QDateTime>
#include <QDebug>

class CTimer
{
public:
    CTimer() {};
    ~CTimer() {};

    int64_t getAudioTimeGap()
    {
        int64_t now = QDateTime::currentMSecsSinceEpoch();
        int64_t ret = now - lastAudioTime;
        // qDebug() << "lastAudioTime" << lastAudioTime << "now" << now << "gap:" << ret;
        lastAudioTime = now;
        return ret;
    }
    int64_t getVideoTimeGap()
    {
        int64_t now = QDateTime::currentMSecsSinceEpoch();
        int64_t ret = now - lastVideoTime;
        lastVideoTime = now;
        return ret;
    }

private:
    int64_t lastAudioTime{0};
    int64_t lastVideoTime{0};
    int64_t stopTime;
};

enum CONTL_TYPE
{
    NONE,
    PLAY,
    PAUSE,
    RESUME,
    END,
};