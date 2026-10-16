#include "MpvVideo.h"
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickOpenGLUtils>
#include <QPointer>
#include <QtGui/qguiapplication_platform.h>
#include <mpv/render_gl.h>

class VideoRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit VideoRenderer(const MpvVideo *item) : m_item(const_cast<MpvVideo *>(item)), m_core(item->player()->core()) {
        mpv_opengl_init_params gl{[](void *, const char *name) -> void * {
            return reinterpret_cast<void *>(QOpenGLContext::currentContext()->getProcAddress(QByteArray(name)));
        }, nullptr};
        mpv_render_param params[5] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl}, {MPV_RENDER_PARAM_INVALID, nullptr},
            {MPV_RENDER_PARAM_INVALID, nullptr}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        int index = 2;
#if QT_CONFIG(xcb)
        if (auto *native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
            params[index++] = {MPV_RENDER_PARAM_X11_DISPLAY, native->display()};
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && QT_CONFIG(wayland)
        if (auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
            params[index++] = {MPV_RENDER_PARAM_WL_DISPLAY, native->display()};
#endif
        const int result = mpv_render_context_create(&m_render, m_core->handle, params);
        auto *player = item->player();
        if (result < 0) {
            QMetaObject::invokeMethod(player, [player] { player->rendererFailed(); }, Qt::QueuedConnection);
            return;
        }
        mpv_render_context_set_update_callback(m_render, [](void *context) {
            auto *self = static_cast<VideoRenderer *>(context);
            if (self->m_item) QMetaObject::invokeMethod(self->m_item, "update", Qt::QueuedConnection);
        }, this);
        QMetaObject::invokeMethod(player, [player] { player->rendererReady(); }, Qt::QueuedConnection);
    }
    ~VideoRenderer() override {
        if (m_render) {
            mpv_render_context_set_update_callback(m_render, nullptr, nullptr);
            mpv_render_context_free(m_render);
        }
    }
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        return new QOpenGLFramebufferObject(size);
    }
    void render() override {
        if (m_render) {
            mpv_render_context_update(m_render);
            auto *fbo = framebufferObject();
            mpv_opengl_fbo target{int(fbo->handle()), fbo->width(), fbo->height(), 0};
            // mpv's FBO output matches Qt Quick's texture orientation. Keep
            // this flip and the item's mirrorVertically property disabled.
            int flip = 0;
            mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &target}, {MPV_RENDER_PARAM_FLIP_Y, &flip}, {MPV_RENDER_PARAM_INVALID, nullptr}};
            mpv_render_context_render(m_render, params);
        } else {
            auto *gl = QOpenGLContext::currentContext()->functions();
            gl->glClearColor(0, 0, 0, 1); gl->glClear(GL_COLOR_BUFFER_BIT);
        }
        QQuickOpenGLUtils::resetOpenGLState();
    }
private:
    QPointer<MpvVideo> m_item;
    std::shared_ptr<MpvCore> m_core;
    mpv_render_context *m_render = nullptr;
};
QQuickFramebufferObject::Renderer *MpvVideo::createRenderer() const { return new VideoRenderer(this); }
