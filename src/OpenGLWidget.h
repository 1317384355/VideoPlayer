#pragma once
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>

extern "C"
{
#include <libavutil/pixfmt.h>
}

class BaseOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
private:
    QOpenGLShaderProgram *program{nullptr};
    QOpenGLBuffer vbo;
    const void *vertices{nullptr};
    const char *fsrc{nullptr};

    virtual void initializeGL() override;
    virtual void paintGL() override { paint(); }

protected:
    uint8_t *dataPtr{nullptr};
    int videoW, videoH;

    // void setVerticesAndFsrc(const void *_vertices, const char *_fsrc);
    // 设置顶点坐标和片段着色器源码
    void loadPixelFormat(AVPixelFormat pixelFormat);
    // 初始化纹理, 必须在子类中实现
    virtual void initTexture() = 0;
    virtual void paint() = 0;
    // 加载分量纹理
    void loadTexture(GLenum textureType, GLuint textureId, GLsizei width, GLsizei height, GLenum format, const GLvoid *pixels);

    int programUniformLocation(const char *name) { return program->uniformLocation(name); }

public:
    BaseOpenGLWidget(AVPixelFormat pixelFormat, QWidget *parent = nullptr) : QOpenGLWidget(parent) { loadPixelFormat(pixelFormat); }
    ~BaseOpenGLWidget() override {}

    void setPixelData(uint8_t *pixelData, int pixelWidth, int pixelHeight);
};

class Yuv420GLWidget : public BaseOpenGLWidget
{
private:
    GLuint textureUniformY, textureUniformU, textureUniformV; // opengl中y、u、v分量位置
    QOpenGLTexture *textureY = nullptr, *textureU = nullptr, *textureV = nullptr;
    GLuint idY, idU, idV; // 自己创建的纹理对象ID，创建错误返回0

protected:
    virtual void initTexture() override;
    virtual void paint() override;

public:
    Yuv420GLWidget(QWidget *parent = nullptr) : BaseOpenGLWidget(AV_PIX_FMT_YUV420P, parent) { qDebug() << "Yuv420GLWidget"; }
};

class Nv12GLWidget : public BaseOpenGLWidget
{
private:
    GLuint textureUniformY, textureUniformUV; // opengl中y、u、v分量位置
    QOpenGLTexture *textureY = nullptr, *textureUV = nullptr;
    GLuint idY, idUV;

protected:
    virtual void initTexture() override;
    virtual void paint() override;

public:
    Nv12GLWidget(QWidget *parent = nullptr) : BaseOpenGLWidget(AV_PIX_FMT_NV12, parent) { qDebug() << "Nv12GLWidget"; }

    void setPixelData(uint8_t *pixelData, int pixelWidth, int pixelHeight)
    {
        if (dataPtr)
            delete[] dataPtr; // 释放之前的内存
        dataPtr = pixelData;
        videoW = pixelWidth;
        videoH = pixelHeight;
        update();
    }
};
