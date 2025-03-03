QT       += core gui multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS +=              \
    src/demo.h          \
    src/CMediaDialog.h  \
    src/Decode.h        \
    src/AudioRenderer.h \
    src/VideoWaiter.h   \
    src/OpenGLWidget.h  \
    src/playerCommand.h \

SOURCES +=                  \
    src/demo.cpp            \
    src/CMediaDialog.cpp    \
    src/Decode.cpp          \
    src/AudioRenderer.cpp   \
    src/VideoWaiter.cpp     \
    src/OpenGLWidget.cpp    \
    src/main.cpp            \

# $$PWD 表示pro文件的当前路径
LIBS += $$PWD/lib/ffmpeg/lib/*.lib
INCLUDEPATH += $$PWD/lib/ffmpeg/include
DEPENDPATH += $$PWD/lib/ffmpeg/include

DESTDIR = $$PWD/bin
