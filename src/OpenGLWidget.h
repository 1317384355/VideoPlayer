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
#include <memory>

class BaseOpenGLWidget : public QOpenGLWidget
{
private:
    QOpenGLBuffer vbo;
    QOpenGLShaderProgram *program{nullptr};

protected:
    std::unique_ptr<uint8_t> dataPtr;
    GLsizei videoW, videoH;
    float videoRatio = 1.0f;

    GLint x, y;
    GLsizei viewW, viewH;
    float widgetRatio = 1.0f;

    void initShader(const void *vertices, int count, const char *fsrc);

    GLuint programUniformLocation(const char *name) { return program->uniformLocation(name); }

public:
    BaseOpenGLWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {}

    void setPixelData(uint8_t *pixelData, int width, int height);
};

class Nv12GLWidget : public BaseOpenGLWidget, protected QOpenGLFunctions
{
private:
    GLuint textureUniformY, textureUniformUV; // opengl中y、u、v分量位置
    GLuint idY, idUV;

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;

public:
    Nv12GLWidget(QWidget *parent = nullptr) : BaseOpenGLWidget(parent) {}
};

class Yuv420GLWidget : public BaseOpenGLWidget, protected QOpenGLFunctions
{
private:
    GLuint textureUniformY, textureUniformU, textureUniformV; // opengl中y、u、v分量位置
    GLuint idY, idU, idV;

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;

public:
    Yuv420GLWidget(QWidget *parent = nullptr) : BaseOpenGLWidget(parent) {}
};
