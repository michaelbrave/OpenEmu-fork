#include "oe_emulator_view.h"
#include <QMutexLocker>

namespace OpenEmu {

EmulatorView::EmulatorView(QWidget *parent)
    : QOpenGLWidget(parent), m_program(nullptr), m_textureId(0), m_hasFrame(false)
{
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_pixelFormat = OE_PIXEL_RGBA8888;
}

EmulatorView::~EmulatorView()
{
    makeCurrent();
    if (m_textureId) glDeleteTextures(1, &m_textureId);
    delete m_program;
    m_vbo.destroy();
    doneCurrent();
}

void EmulatorView::updateFrame(const void* buffer, int width, int height, OEPixelFormat format)
{
    QMutexLocker locker(&m_frameMutex);
    m_videoWidth = width;
    m_videoHeight = height;
    m_pixelFormat = format;

    size_t size = 0;
    if (format == OE_PIXEL_RGBA8888) size = width * height * 4;
    else if (format == OE_PIXEL_RGB565) size = width * height * 2;
    else if (format == OE_PIXEL_INDEXED8) size = width * height; // Requires palette logic later

    if (m_frameBuffer.size() != size) {
        m_frameBuffer.resize(size);
    }
    memcpy(m_frameBuffer.data(), buffer, size);
    m_hasFrame = true;
    locker.unlock();

    update(); // Request a repaint
}

void EmulatorView::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    setupShaders();

    static const float vertices[] = {
        // Pos      // Tex
        -1.0f,  1.0f,  0.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f
    };

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void EmulatorView::setupShaders()
{
    m_program = new QOpenGLShaderProgram();
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
        "attribute vec4 a_position;\n"
        "attribute vec2 a_texCoord;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "   gl_Position = a_position;\n"
        "   v_texCoord = a_texCoord;\n"
        "}\n");
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
        "uniform sampler2D u_texture;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "   vec4 color = texture2D(u_texture, v_texCoord);\n"
        "   gl_FragColor = vec4(color.rgb, 1.0);\n"
        "}\n");
    m_program->bindAttributeLocation("a_position", 0);
    m_program->bindAttributeLocation("a_texCoord", 1);
    m_program->link();
}

void EmulatorView::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    QMutexLocker locker(&m_frameMutex);
    if (!m_hasFrame || m_videoWidth <= 0 || m_videoHeight <= 0) return;

    m_program->bind();
    m_vbo.bind();
    glBindTexture(GL_TEXTURE_2D, m_textureId);

    GLint internalFormat;
    GLenum format;
    GLenum type;

    if (m_pixelFormat == OE_PIXEL_RGBA8888) {
        internalFormat = GL_RGBA8;
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
    } else if (m_pixelFormat == OE_PIXEL_RGB565) {
        internalFormat = GL_RGB8;
        format = GL_RGB;
        type = GL_UNSIGNED_SHORT_5_6_5;
    } else {
        return;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_videoWidth, m_videoHeight, 0, format, type, m_frameBuffer.data());

    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));
    m_program->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_program->disableAttributeArray(0);
    m_program->disableAttributeArray(1);
    m_program->release();
}

void EmulatorView::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

} // namespace OpenEmu
