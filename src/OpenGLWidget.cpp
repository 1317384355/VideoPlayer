#include "OpenGLWidget.h"

#define VERTEXIN 0
#define TEXTUREIN 1

const char *vsrc =
    "attribute vec4 vertexIn; \n"
    "attribute vec4 textureIn; \n"
    "varying vec2 textureOut;  \n"
    "void main(void)           \n"
    "{                         \n"
    "    gl_Position = vertexIn; \n"
    "    textureOut = textureIn; \n"
    "}\n";

const char *nv12_fsrc =
    "varying vec2 textureOut;\n"
    "uniform sampler2D textureY;\n"
    "uniform sampler2D textureUV;\n"
    "void main(void)\n"
    "{\n"
    "    vec3 yuv; \n"
    "    vec3 rgb; \n"
    "    yuv.x = texture2D(textureY, textureOut.st).r - 0.0625; \n"
    "    yuv.y = texture2D(textureUV, textureOut.st).r - 0.5; \n"
    "    yuv.z = texture2D(textureUV, textureOut.st).g - 0.5; \n"
    "    rgb = mat3( 1,       1,         1,         \n"
    "                0,       -0.39465,  2.03211,   \n"
    "                1.13983, -0.58060,  0) * yuv;  \n"
    "    gl_FragColor = vec4(rgb, 1); \n"
    "}\n";
const char *yuv420_fsrc =
    "varying vec2 textureOut; \n"
    "uniform sampler2D textureY; \n"
    "uniform sampler2D textureU; \n"
    "uniform sampler2D textureV; \n"
    "void main(void) \n"
    "{ \n"
    "    vec3 yuv; \n"
    "    vec3 rgb; \n"
    "    yuv.x = texture2D(textureY, textureOut).r; \n"
    "    yuv.y = texture2D(textureU, textureOut).r - 0.5; \n"
    "    yuv.z = texture2D(textureV, textureOut).r - 0.5; \n"
    "    rgb = mat3( 1,       1,         1, \n"
    "                0,       -0.39465,  2.03211, \n"
    "                1.13983, -0.58060,  0) * yuv; \n"
    "    gl_FragColor = vec4(rgb, 1); \n"
    "  }\n";

static const GLfloat yuv420_vertices[]{
    // 顶点坐标
    -1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f, +1.0f, -1.0f,
    // 纹理坐标
    0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};

static const GLfloat nv12_vertices[]{
    -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f,

    0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

void initGLFuncs(QOpenGLFunctions *glFuncs, GLclampf red = 0.0f, GLclampf green = 0.0f, GLclampf blue = 0.0f, GLclampf alpha = 0.0f)
{
    if (!glFuncs)
        return;

    glFuncs->initializeOpenGLFunctions();
    glFuncs->glEnable(GL_DEPTH_TEST);
    glFuncs->glClearColor(red, green, blue, alpha);
}

void loadTexture(QOpenGLFunctions *glFuncs, GLenum textureType, GLuint textureId, GLsizei width, GLsizei height, GLenum format, const GLvoid *pixels)
{
    glFuncs->glActiveTexture(textureType);
    glFuncs->glBindTexture(GL_TEXTURE_2D, textureId);
    glFuncs->glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void BaseOpenGLWidget::initShader(const void *vertices, int count, const char *fsrc)
{
    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, count);

    program = new QOpenGLShaderProgram(this);
    program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);

    program->bindAttributeLocation("vertexIn", VERTEXIN);
    program->bindAttributeLocation("textureIn", TEXTUREIN);
    program->link();
    program->bind();
    program->enableAttributeArray(VERTEXIN);
    program->enableAttributeArray(TEXTUREIN);
    program->setAttributeBuffer(VERTEXIN, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    program->setAttributeBuffer(TEXTUREIN, GL_FLOAT, 8 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));
}

void BaseOpenGLWidget::setPixelData(uint8_t *pixelData, int width, int height)
{
    dataPtr = pixelData;
    videoW = width;
    videoH = height;
    update();
}

void Nv12GLWidget::initializeGL()
{
    initGLFuncs(this);
    initShader(nv12_vertices, sizeof(nv12_vertices), nv12_fsrc);
    textureUniformY = programUniformLocation("textureY");
    textureUniformUV = programUniformLocation("textureUV");
    auto textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    auto textureUV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureY->create();
    textureUV->create();
    idY = textureY->textureId();
    idUV = textureUV->textureId();
}

void Nv12GLWidget::paintGL()
{
    if (dataPtr == nullptr)
        return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 注释后画面卡死
    glDisable(GL_DEPTH_TEST);                           // 关闭深度测试, 注释后内存占用增加
    glViewport(0, 0, width(), height());

    loadTexture(this, GL_TEXTURE0, idY, videoW, videoH, GL_RED, dataPtr);
    loadTexture(this, GL_TEXTURE1, idUV, videoW / 2, videoH / 2, GL_RG, dataPtr + videoW * videoH);

    glUniform1i(textureUniformY, 0);
    glUniform1i(textureUniformUV, 1);
    // glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //  GL_TRIANGLE_STRIP会导致屏幕左侧出现一个三角形
    // glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // 与GL_QUADS效果相同, 原因暂不清楚
    glDrawArrays(GL_QUADS, 0, 4);

    delete[] dataPtr;
    dataPtr = nullptr;
}

void Yuv420GLWidget::initializeGL()
{
    initGLFuncs(this);
    initShader(yuv420_vertices, sizeof(yuv420_vertices), yuv420_fsrc);
    textureUniformY = programUniformLocation("textureY");
    textureUniformU = programUniformLocation("textureU");
    textureUniformV = programUniformLocation("textureV");
    auto textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    auto textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    auto textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureY->create();
    textureU->create();
    textureV->create();
    idY = textureY->textureId();
    idU = textureU->textureId();
    idV = textureV->textureId();
}

void Yuv420GLWidget::paintGL()
{
    if (dataPtr == nullptr)
        return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 注释后画面卡死
    glDisable(GL_DEPTH_TEST);                           // 关闭深度测试, 注释后内存占用增加
    glViewport(0, 0, width(), height());

    int halfW = videoW >> 1;
    int halfH = videoH >> 1;

    loadTexture(this, GL_TEXTURE0, idY, videoW, videoH, GL_RED, dataPtr);
    loadTexture(this, GL_TEXTURE1, idU, halfW, halfH, GL_RED, dataPtr + videoW * videoH);
    loadTexture(this, GL_TEXTURE2, idV, halfW, halfH, GL_RED, dataPtr + halfW * halfH * 5);

    glUniform1i(textureUniformY, 0);
    glUniform1i(textureUniformU, 1);
    glUniform1i(textureUniformV, 2);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    delete[] dataPtr;
    dataPtr = nullptr;
}
