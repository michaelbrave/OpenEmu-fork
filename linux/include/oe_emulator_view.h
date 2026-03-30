#ifndef OE_EMULATOR_VIEW_H
#define OE_EMULATOR_VIEW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QMutex>
#include "oe_core_interface.h"

namespace OpenEmu {

class EmulatorView : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    EmulatorView(QWidget *parent = nullptr);
    ~EmulatorView();

    void updateFrame(const void* buffer, int width, int height, OEPixelFormat format);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    QOpenGLShaderProgram *m_program;
    QOpenGLBuffer m_vbo;
    GLuint m_textureId;
    
    QMutex m_frameMutex;
    std::vector<uint8_t> m_frameBuffer;
    int m_videoWidth;
    int m_videoHeight;
    OEPixelFormat m_pixelFormat;
    bool m_hasFrame;

    void setupShaders();
};

} // namespace OpenEmu

#endif // OE_EMULATOR_VIEW_H
