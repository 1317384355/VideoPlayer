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
    "uniform sampler2D textureY;\n"
    "uniform sampler2D textureUV;\n"
    "varying mediump vec2 textureOut;\n"
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
    "gl_FragColor = vec4(rgb, 1); \n"
    "}\n";
const char *yuv_fsrc = "varying vec2 textureOut; \
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

static const GLfloat yuv_vertices[]{
    // 顶点坐标
    -1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f, +1.0f, -1.0f,
    // 纹理坐标
    0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};

static const GLfloat nv12_vertices[]{
    -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f,

    0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

void BaseOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

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

    initTexture();
}

void BaseOpenGLWidget::loadPixelFormat(AVPixelFormat pixelFormat)
{
    switch (pixelFormat)
    {
    case AV_PIX_FMT_YUV420P:
        vertices = yuv_vertices;
        fsrc = yuv_fsrc;
        break;
    case AV_PIX_FMT_NV12:
        vertices = nv12_vertices;
        fsrc = nv12_fsrc;
        break;
    default:
        break;
    }
}

void BaseOpenGLWidget::loadTexture(GLenum textureType, GLuint textureId, GLsizei width, GLsizei height, GLenum format, const GLvoid *pixels)
{
    glActiveTexture(textureType);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void BaseOpenGLWidget::setPixelData(uint8_t *pixelData, int pixelWidth, int pixelHeight)
{
    dataPtr = pixelData;
    pixelWidth = pixelWidth;
    pixelHeight = pixelHeight;
    // qDebug() << "ptr-setPixelData:" << dataPtr;
    update();
}

void Yuv420GLWidget::initTexture()
{
    textureUniformY = programUniformLocation("tex_y");
    textureUniformU = programUniformLocation("tex_u");
    textureUniformV = programUniformLocation("tex_v");
    textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureY->create();
    textureU->create();
    textureV->create();
    idY = textureY->textureId();
    idU = textureU->textureId();
    idV = textureV->textureId();
    glClearColor(0.1f, 0.5f, 0.9f, 0.0f);
}

void Yuv420GLWidget::paint()
{
    if (!dataPtr)
        return;

    int halfWidth = videoW >> 1;
    int halfHeight = videoH >> 1;
    glViewport(0, 0, width(), height());

    loadTexture(GL_TEXTURE0, idY, videoW, videoH, GL_RED, dataPtr);
    loadTexture(GL_TEXTURE1, idU, halfWidth, halfHeight, GL_RED, dataPtr + videoW * videoH);
    loadTexture(GL_TEXTURE2, idV, halfWidth, halfHeight, GL_RED, dataPtr + halfWidth * halfHeight * 5);

    // 指定y纹理要使用新值
    glUniform1i(textureUniformY, 0);
    // 指定u纹理要使用新值
    glUniform1i(textureUniformU, 1);
    // 指定v纹理要使用新值
    glUniform1i(textureUniformV, 2);
    // 使用顶点数组方式绘制图形
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    // 高亮当前选中窗口

    delete[] dataPtr;
    dataPtr = nullptr;
}

void Nv12GLWidget::initTexture()
{
    textureUniformY = programUniformLocation("tex_y");
    textureUniformUV = programUniformLocation("tex_uv");
    textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureUV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureY->create();
    textureUV->create();
    idY = textureY->textureId();
    idUV = textureUV->textureId();
    glClearColor(0.99f, 0.99f, 0.99f, 0.0f);
}

void Nv12GLWidget::paint()
{
    if (!dataPtr)
        return;
    glViewport(0, 0, width(), height());

    // glClearColor(0.5f, 0.5f, 0.7f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST); // 关闭深度测试

    loadTexture(GL_TEXTURE0, idY, videoW, videoH, GL_RED, dataPtr);
    loadTexture(GL_TEXTURE1, idUV, videoW >> 1, videoH >> 1, GL_RG, dataPtr + videoW * videoH);

    glUniform1i(textureUniformY, 0);
    glUniform1i(textureUniformUV, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    delete[] dataPtr;
    dataPtr = nullptr;
}

// glActiveTexture(GL_TEXTURE0);      // 激活纹理单元GL_TEXTURE0,系统里面的
// glBindTexture(GL_TEXTURE_2D, idY); // 绑定y分量纹理对象id到激活的纹理单元
// // 使用内存中的数据创建真正的y分量纹理数据
// glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW, videoH, 0, GL_RED, GL_UNSIGNED_BYTE, dataPtr);
// // https://blog.csdn.net/xipiaoyouzi/article/details/53584798 纹理参数解析
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

// glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
// glBindTexture(GL_TEXTURE1, idU);
// // 使用内存中的数据创建真正的u分量纹理数据
// glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, halfWidth, halfHeight, 0, GL_RED, GL_UNSIGNED_BYTE, dataPtr + videoW * videoH);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

// glActiveTexture(GL_TEXTURE2); // 激活纹理单元GL_TEXTURE2
// glBindTexture(GL_TEXTURE_2D, idV);
// // 使用内存中的数据创建真正的v分量纹理数据
// glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, halfWidth, halfHeight, 0, GL_RED, GL_UNSIGNED_BYTE, dataPtr + videoW * videoH * 5 / 4);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);