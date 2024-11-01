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
    src/audioThread.h \
    src/demo.h \
    src/playerCommand.h \
    src/videoThread.h

SOURCES += \
    src/audioThread.cpp \
    src/demo.cpp \
    src/main.cpp \
    src/videoThread.cpp

LIBS += D:/lib/OpenCV/x64/mingw/lib/libopencv_*.a
INCLUDEPATH += D:/lib/OpenCV/include
DEPENDPATH += D:/lib/OpenCV/include

LIBS += D:/lib/ffmpeg/lib/*.a
INCLUDEPATH += D:/lib/ffmpeg/include
DEPENDPATH += D:/lib/ffmpeg/include
