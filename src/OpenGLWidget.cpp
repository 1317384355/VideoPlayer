#include "OpenGLWidget.h"
#define VERTEXIN 0
#define TEXTUREIN 1

void Yuv420Funcs::initialize()
{
    qDebug() << "Yuv420Funcs::initialize";
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
    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex);
    const char *vsrc = "\
        attribute vec4 vertexIn; \
        attribute vec2 textureIn; \
        varying vec2 textureOut;  \
        void main(void)           \
    {                         \
            gl_Position = vertexIn; \
            textureOut = textureIn; \
    }";
    vshader->compileSourceCode(vsrc);
    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment);
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

    program = new QOpenGLShaderProgram;
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

void Yuv420Funcs::render(uint8_t *pixelData, int pixelWidth, int pixelHeight)
{
    if (!pixelData)
        return;
    qDebug() << "ptr-paint" << pixelData;

    int halfWidth = pixelWidth >> 1;
    int halfHeight = pixelHeight >> 1;
    qDebug() << parentPtr()->width() << parentPtr()->height();
    glViewport(0, 0, parentPtr()->width(), parentPtr()->height());
    glActiveTexture(GL_TEXTURE0);      // 激活纹理单元GL_TEXTURE0,系统里面的
    glBindTexture(GL_TEXTURE_2D, idY); // 绑定y分量纹理对象id到激活的纹理单元
    // 使用内存中的数据创建真正的y分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixelWidth, pixelHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pixelData);
    // https://blog.csdn.net/xipiaoyouzi/article/details/53584798 纹理参数解析
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
    glBindTexture(GL_TEXTURE1, idU);
    // 使用内存中的数据创建真正的u分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, halfWidth, halfHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pixelData + pixelWidth * pixelHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE2); // 激活纹理单元GL_TEXTURE2
    glBindTexture(GL_TEXTURE_2D, idV);
    // 使用内存中的数据创建真正的v分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, halfWidth, halfHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pixelData + pixelWidth * pixelHeight * 5 / 4);
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
}

void Nv12Funcs::initialize()
{
    qDebug() << "initialize";
    initializeOpenGLFunctions();
    const char *vsrc =
        "attribute vec4 vertexIn; \
             attribute vec4 textureIn; \
             varying vec4 textureOut;  \
             void main(void)           \
             {                         \
                 gl_Position = vertexIn; \
                 textureOut = textureIn; \
             }";

    const char *fsrc =
        "varying mediump vec4 textureOut;\n"
        "uniform sampler2D textureY;\n"
        "uniform sampler2D textureUV;\n"
        "void main(void)\n"
        "{\n"
        "vec3 yuv; \n"
        "vec3 rgb; \n"
        "yuv.x = texture2D(textureY, textureOut.st).r - 0.0625; \n"
        "yuv.y = texture2D(textureUV, textureOut.st).r - 0.5; \n"
        "yuv.z = texture2D(textureUV, textureOut.st).g - 0.5; \n"
        "rgb = mat3( 1,       1,         1, \n"
        "0,       -0.39465,  2.03211, \n"
        "1.13983, -0.58060,  0) * yuv; \n"
        "gl_FragColor = vec4(rgb, 1); \n"
        "}\n";

    program.addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    program.addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    program.link();

    GLfloat points[]{
        -1.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, -1.0f,
        -1.0f, -1.0f,

        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f};

    vbo.create();
    vbo.bind();
    vbo.allocate(points, sizeof(points));

    GLuint ids[2];
    glGenTextures(2, ids);
    idY = ids[0];
    idUV = ids[1];
}

void Nv12Funcs::render(uint8_t *pixelData, int pixelWidth, int pixelHeight)
{
    if (!pixelData)
        return;
    qDebug() << "ptr-paint" << pixelData;
    glClearColor(0.5f, 0.5f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    program.bind();
    vbo.bind();
    program.enableAttributeArray("vertexIn");
    program.enableAttributeArray("textureIn");
    program.setAttributeBuffer("vertexIn", GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    program.setAttributeBuffer("textureIn", GL_FLOAT, 2 * 4 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, idY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixelWidth, pixelHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pixelData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, idUV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, pixelWidth >> 1, pixelHeight >> 1, 0, GL_RG, GL_UNSIGNED_BYTE, pixelData + pixelWidth * pixelHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    program.setUniformValue("textureUV", 0);
    program.setUniformValue("textureY", 1);
    glDrawArrays(GL_QUADS, 0, 4);
    program.disableAttributeArray("vertexIn");
    program.disableAttributeArray("textureIn");
    program.release();
}

void COpenGLWidget::setPixelData(uint8_t *pixelData, int pixelWidth, int pixelHeight)
{
    if (dataPtr)
        delete[] dataPtr;

    dataPtr = pixelData;
    pixelWidth = pixelWidth;
    pixelHeight = pixelHeight;
    // qDebug() << "ptr-setPixelData:" << dataPtr;
    update();
}

void COpenGLWidget::initGLFuncs(int pixFmt)
{
    if (glFuncs)
    {
        delete glFuncs;
        glFuncs = nullptr;
    }
    switch (pixFmt)
    {
    case AV_PIX_FMT_NONE:
        glFuncs = new Yuv420Funcs();
        break;
    case AV_PIX_FMT_CUDA:
        glFuncs = new Nv12Funcs();
        break;
    default:
        break;
    }
    glFuncs->setParent(this);
}

void Yuv420GLWidget::initializeGL()
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
    const char *vsrc = "\
        attribute vec4 vertexIn; \
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

void Yuv420GLWidget::paintGL()
{
    if (dataPtr == nullptr)
        return;

    // qDebug() << "ptr-paintGL:" << dataPtr << dataPtr + videoW * videoH << dataPtr + videoW * videoH * 5 / 4;
    int halfWidth = videoW >> 1;
    int halfHeight = videoH >> 1;
    qDebug() << width() << height();
    glViewport(0, 0, width(), height());
    glActiveTexture(GL_TEXTURE0);      // 激活纹理单元GL_TEXTURE0,系统里面的
    glBindTexture(GL_TEXTURE_2D, idY); // 绑定y分量纹理对象id到激活的纹理单元
    // 使用内存中的数据创建真正的y分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW, videoH, 0, GL_RED, GL_UNSIGNED_BYTE, dataPtr);
    // https://blog.csdn.net/xipiaoyouzi/article/details/53584798 纹理参数解析
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
    glBindTexture(GL_TEXTURE1, idU);
    // 使用内存中的数据创建真正的u分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, halfWidth, halfHeight, 0, GL_RED, GL_UNSIGNED_BYTE, dataPtr + videoW * videoH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE2); // 激活纹理单元GL_TEXTURE2
    glBindTexture(GL_TEXTURE_2D, idV);
    // 使用内存中的数据创建真正的v分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, halfWidth, halfHeight, 0, GL_RED, GL_UNSIGNED_BYTE, dataPtr + videoW * videoH * 5 / 4);
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
}

void Nv12GLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    const char *vsrc =
        "attribute vec4 vertexIn; \
             attribute vec4 textureIn; \
             varying vec4 textureOut;  \
             void main(void)           \
             {                         \
                 gl_Position = vertexIn; \
                 textureOut = textureIn; \
             }";

    const char *fsrc =
        "varying mediump vec4 textureOut;\n"
        "uniform sampler2D textureY;\n"
        "uniform sampler2D textureUV;\n"
        "void main(void)\n"
        "{\n"
        "vec3 yuv; \n"
        "vec3 rgb; \n"
        "yuv.x = texture2D(textureY, textureOut.st).r - 0.0625; \n"
        "yuv.y = texture2D(textureUV, textureOut.st).r - 0.5; \n"
        "yuv.z = texture2D(textureUV, textureOut.st).g - 0.5; \n"
        "rgb = mat3( 1,       1,         1, \n"
        "0,       -0.39465,  2.03211, \n"
        "1.13983, -0.58060,  0) * yuv; \n"
        "gl_FragColor = vec4(rgb, 1); \n"
        "}\n";

    program.addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    program.addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    program.link();

    GLfloat points[]{
        -1.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, -1.0f,
        -1.0f, -1.0f,

        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f};

    vbo.create();
    vbo.bind();
    vbo.allocate(points, sizeof(points));

    GLuint ids[2];
    glGenTextures(2, ids);
    idY = ids[0];
    idUV = ids[1];
}

void Nv12GLWidget::paintGL()
{
    if (!dataPtr)
        return;
    qDebug() << "ptr-paint" << dataPtr;
    glClearColor(0.5f, 0.5f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    program.bind();
    vbo.bind();
    program.enableAttributeArray("vertexIn");
    program.enableAttributeArray("textureIn");
    program.setAttributeBuffer("vertexIn", GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    program.setAttributeBuffer("textureIn", GL_FLOAT, 2 * 4 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, idY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW, videoH, 0, GL_RED, GL_UNSIGNED_BYTE, dataPtr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, idUV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, videoW >> 1, videoH >> 1, 0, GL_RG, GL_UNSIGNED_BYTE, dataPtr + videoW * videoH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    program.setUniformValue("textureUV", 0);
    program.setUniformValue("textureY", 1);
    glDrawArrays(GL_QUADS, 0, 4);
    program.disableAttributeArray("vertexIn");
    program.disableAttributeArray("textureIn");
    program.release();
}
