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

HEADERS += \
    src/CMediaDialog.h \
    src/AudioThread.h \
    src/Decode.h \
    src/OpenGLWidget.h \
    src/demo.h \
    src/playerCommand.h \
    src/VideoThread.h

SOURCES += \
    src/CMediaDialog.cpp \
    src/AudioThread.cpp \
    src/Decode.cpp \
    src/OpenGLWidget.cpp \
    src/demo.cpp \
    src/main.cpp \
    src/VideoThread.cpp

# $$PWD 表示pro文件的当前路径
LIBS += $$PWD/lib/ffmpeg/lib/*.lib
INCLUDEPATH += $$PWD/lib/ffmpeg/include
DEPENDPATH += $$PWD/lib/ffmpeg/include

DESTDIR = $$PWD/exe_path
