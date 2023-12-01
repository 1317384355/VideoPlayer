#pragma once
#include <QDebug>
#include <QThread>
#include <QDateTime>
#include <QTimer>
#include <QTime>
#include <QElapsedTimer>

enum CONTL_TYPE
{
    NONE,
    PLAY,
    PAUSE,
    RESUME,
    END,
};

// 返回当前系统时和 last 的差值 并将last 更新为当前系统时
inline int getElapsed_AndUpdate(int &last, QTime *time)
{
    int cur = time->msecsSinceStartOfDay();
    int ret = cur - last;
    last = cur;
    return (ret >= 0 ? ret : 0);
}
