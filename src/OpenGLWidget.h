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

class COpenGLFunctions : public QOpenGLFunctions
{
private:
    QOpenGLWidget *m_parent = nullptr;

protected:
    QOpenGLWidget *parentPtr() { return m_parent; }

public:
    COpenGLFunctions() = default;
    COpenGLFunctions(const COpenGLFunctions &) = delete;

    void setParent(QOpenGLWidget *parent) { m_parent = parent; }

    virtual void initialize() = 0;
    virtual void render(uint8_t *pixelData, int pixelWidth, int pixelHeight) = 0;
};

class COpenGLWidget : public QOpenGLWidget
{
private:
    int videoW, videoH;
    uint8_t *dataPtr = nullptr;
    COpenGLFunctions *glFuncs = nullptr;

protected:
    virtual void initializeGL() override { glFuncs->initialize(); }
    virtual void paintGL() override { glFuncs->render(dataPtr, videoW, videoH); }

public:
    COpenGLWidget(QWidget *parent = nullptr, int pixFmt = AV_PIX_FMT_YUV420P) : QOpenGLWidget(parent) { initGLFuncs(pixFmt); }
    ~COpenGLWidget() override {}

    void initGLFuncs(int pixFmt);
    void setPixelData(uint8_t *pixelData, int pixelWidth, int pixelHeight);
};

class Yuv420Funcs : public COpenGLFunctions
{
private:
    QOpenGLShaderProgram *program;
    QOpenGLBuffer vbo;
    GLuint textureUniformY, textureUniformU, textureUniformV; // opengl中y、u、v分量位置
    QOpenGLTexture *textureY = nullptr, *textureU = nullptr, *textureV = nullptr;
    GLuint idY, idU, idV; // 自己创建的纹理对象ID，创建错误返回0

public:
    virtual void initialize() override;
    virtual void render(uint8_t *pixelData, int pixelWidth, int pixelHeight) override;
};

class Nv12Funcs : public COpenGLFunctions
{
public:
    virtual void initialize() override;
    virtual void render(uint8_t *pixelData, int pixelWidth, int pixelHeight) override;

private:
    QOpenGLShaderProgram program;
    GLuint idY, idUV;
    QOpenGLBuffer vbo;
};

class Yuv420GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
private:
    QOpenGLShaderProgram *program;
    QOpenGLBuffer vbo;
    GLuint textureUniformY, textureUniformU, textureUniformV; // opengl中y、u、v分量位置
    QOpenGLTexture *textureY = nullptr, *textureU = nullptr, *textureV = nullptr;
    GLuint idY, idU, idV; // 自己创建的纹理对象ID，创建错误返回0

    int videoW, videoH;
    uint8_t *dataPtr = nullptr;

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;

public:
    Yuv420GLWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {}

    void setPixelData(uint8_t *pixelData, int pixelWidth, int pixelHeight)
    {
        dataPtr = pixelData;
        videoW = pixelWidth;
        videoH = pixelHeight;
        update();
        // qDebug() << "update";
    }
};

class Nv12GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
private:
    QOpenGLShaderProgram program;
    GLuint idY, idUV;
    QOpenGLBuffer vbo;

    int videoW, videoH;
    uint8_t *dataPtr = nullptr;

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;

public:
    Nv12GLWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {}

    void setPixelData(uint8_t *pixelData, int pixelWidth, int pixelHeight)
    {
        dataPtr = pixelData;
        videoW = pixelWidth;
        videoH = pixelHeight;
        update();
    }
};
