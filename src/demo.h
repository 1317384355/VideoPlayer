#pragma once

#include "audioThread.h"
#include "videoThread.h"
#include "decode.h"
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSlider>
#include <QWidget>
#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShader>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLContext>

class demo : public QWidget
{
    Q_OBJECT
private:
public:
    demo(QWidget *parent = nullptr);
    ~demo();
};

#define VERTEXIN 0
#define TEXTUREIN 1
// 画面窗口
class FrameWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit FrameWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {}
    ~FrameWidget() {}

public slots:
    void receviceFrame(uint8_t **pixelData, int pixelWidth, int pixelHeight)
    {
        yuvPtr = pixelData;
        videoW = pixelWidth;
        videoH = pixelHeight;
        update();
    }

private:
    QOpenGLShaderProgram *program;
    QOpenGLBuffer vbo;
    GLuint textureUniformY, textureUniformU, textureUniformV; // opengl中y、u、v分量位置
    QOpenGLTexture *textureY = nullptr, *textureU = nullptr, *textureV = nullptr;
    GLuint idY, idU, idV; // 自己创建的纹理对象ID，创建错误返回0
    uint videoW, videoH;
    uint8_t **yuvPtr = nullptr;

    QMenu *menu;

signals:
    void clicked();

protected:
    virtual void initializeGL() override
    {
        initializeOpenGLFunctions();
        glEnable(GL_DEPTH_TEST);
        static const GLfloat vertices[]{
            // 顶点坐标
            -1.0f,
            -1.0f,
            -1.0f,
            +1.0f,
            +1.0f,
            +1.0f,
            +1.0f,
            -1.0f,
            // 纹理坐标
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            1.0f,
        };
        vbo.create();
        vbo.bind();
        vbo.allocate(vertices, sizeof(vertices));
        QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex, this);
        const char *vsrc = "attribute vec4 vertexIn; \
        attribute vec2 textureIn; \
        varying vec2 textureOut;  \
        void main(void)           \
    {                         \
            gl_Position = vertexIn; \
            textureOut = textureIn; \
    }";
        vshader->compileSourceCode(vsrc);
        QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
        const char *fsrc = "varying vec2 textureOut; \
        uniform sampler2D tex_y; \
        uniform sampler2D tex_u; \
        uniform sampler2D tex_v; \
        void main(void) \
    { \
            vec3 yuv; \
            vec3 rgb; \
            yuv.x = texture2D(tex_y, textureOut).r; \
            yuv.y = texture2D(tex_u, textureOut).r - 0.5; \
            yuv.z = texture2D(tex_v, textureOut).r - 0.5; \
            rgb = mat3( 1,       1,         1, \
                   0,       -0.39465,  2.03211, \
                   1.13983, -0.58060,  0) * yuv; \
            gl_FragColor = vec4(rgb, 1); \
    }";
        fshader->compileSourceCode(fsrc);

        program = new QOpenGLShaderProgram(this);
        program->addShader(vshader);
        program->addShader(fshader);
        program->bindAttributeLocation("vertexIn", VERTEXIN);
        program->bindAttributeLocation("textureIn", TEXTUREIN);
        program->link();
        program->bind();
        program->enableAttributeArray(VERTEXIN);
        program->enableAttributeArray(TEXTUREIN);
        program->setAttributeBuffer(VERTEXIN, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
        program->setAttributeBuffer(TEXTUREIN, GL_FLOAT, 8 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));

        textureUniformY = program->uniformLocation("tex_y");
        textureUniformU = program->uniformLocation("tex_u");
        textureUniformV = program->uniformLocation("tex_v");
        textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
        textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
        textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
        textureY->create();
        textureU->create();
        textureV->create();
        idY = textureY->textureId();
        idU = textureU->textureId();
        idV = textureV->textureId();
        glClearColor(0.99f, 0.99f, 0.99f, 0.0f);
    }

    virtual void paintGL() override
    {
        if (!yuvPtr)
            return;
        glViewport(0, 0, width(), height());
        glActiveTexture(GL_TEXTURE0);      // 激活纹理单元GL_TEXTURE0,系统里面的
        glBindTexture(GL_TEXTURE_2D, idY); // 绑定y分量纹理对象id到激活的纹理单元
        // 使用内存中的数据创建真正的y分量纹理数据
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW, videoH, 0, GL_RED, GL_UNSIGNED_BYTE, yuvPtr[0]);
        // https://blog.csdn.net/xipiaoyouzi/article/details/53584798 纹理参数解析
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
        glBindTexture(GL_TEXTURE1, idU);
        // 使用内存中的数据创建真正的u分量纹理数据
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW >> 1, videoH >> 1, 0, GL_RED, GL_UNSIGNED_BYTE, yuvPtr[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glActiveTexture(GL_TEXTURE2); // 激活纹理单元GL_TEXTURE2
        glBindTexture(GL_TEXTURE_2D, idV);
        // 使用内存中的数据创建真正的v分量纹理数据
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW >> 1, videoH >> 1, 0, GL_RED, GL_UNSIGNED_BYTE, yuvPtr[2]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // 指定y纹理要使用新值
        glUniform1i(textureUniformY, 0);
        // 指定u纹理要使用新值
        glUniform1i(textureUniformU, 1);
        // 指定v纹理要使用新值
        glUniform1i(textureUniformV, 2);
        // 使用顶点数组方式绘制图形
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        // 高亮当前选中窗口

        delete yuvPtr[0];
        delete yuvPtr[1];
        delete yuvPtr[2];
        delete yuvPtr;
        yuvPtr = nullptr;
    }

    virtual void mouseReleaseEvent(QMouseEvent *event) override
    {
        // 如果鼠标的点在label内部
        if (event->pos().x() >= 0 && event->pos().x() <= this->width() && event->pos().y() >= 0 && event->pos().y() <= this->height())
        {
            if (event->button() == Qt::LeftButton)
                emit this->clicked();
            else if (event->button() == Qt::RightButton)
                menu->exec(event->globalPos());
        }
    }
};

// 视频进度条
class VideoSlider : public QSlider
{
    Q_OBJECT
private:
    bool isPress{false};
    int lastLocation{0}; // 减少移动时发出切换帧信号次数
    int one_percent{0};

signals:
    void sliderClicked();
    void sliderMoved(int value);
    void sliderReleased();

protected:
    void mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            this->isPress = true;
            // 获取鼠标的位置，这里并不能直接从ev中取值（因为如果是拖动的话，鼠标开始点击的位置没有意义了）
            double pos = event->pos().x() / (double)width();
            setValue(pos * (maximum() - minimum()) + minimum());
            qDebug() << "setValue: " << this->value();
            emit VideoSlider::sliderClicked();
        }
    }

    void mouseMoveEvent(QMouseEvent *event)
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
            //     emit VideoSlider::sliderMoved(value);
            // }
        }
    }

    void mouseReleaseEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            this->isPress = false;
            emit VideoSlider::sliderMoved(this->value());
            qDebug() << "slider_value: " << this->value();

            QSlider::mouseReleaseEvent(event);
            emit VideoSlider::sliderReleased();
        }
    }

public:
    VideoSlider(Qt::Orientation orientation, QWidget *parent = nullptr) : QSlider(orientation, parent) {}
    void setRange(int min, int max)
    {
        one_percent = (max - min) / 100;
        QSlider::setRange(min, max);
    }
    bool getIsPress() { return this->isPress; }
};

class CMediaDialog : public QWidget
{
    Q_OBJECT
signals:
    void startPlay();

private slots:
    // 响应拖动进度条, 当鼠标压下时暂停, 并保存播放状态
    void startSeek();

    // 响应拖动进度条, 当鼠标松开时恢复播放状态
    void endSeek();

    // 强制关闭
    void terminatePlay();

private:
    FrameWidget *frameWidget{nullptr};
    VideoSlider *slider{nullptr};

    Decode *decode_th{nullptr};
    VideoThread *video_th{nullptr};
    AudioThread *audio_th{nullptr};
    QThread *decodeThread{nullptr};
    QThread *videoThread{nullptr};
    QThread *audioThread{nullptr};

    int m_type;

    bool isPlay = false; // 保存拖动进度条前视频播放状态

public:
    CMediaDialog(QWidget *parent = nullptr);
    ~CMediaDialog();

    void showVideo(const QString &path);
    void changePlayState();
};
