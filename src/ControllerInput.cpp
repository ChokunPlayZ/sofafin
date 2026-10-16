#include "ControllerInput.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>
#include <QDesktopServices>
#include <QUrlQuery>

ControllerInput::ControllerInput(QObject *parent) : QObject(parent) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return;
    refresh(); m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, &ControllerInput::poll);
    m_timer.start(16);
}
ControllerInput::~ControllerInput() {
    if (m_controller) SDL_GameControllerClose(m_controller);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}
QUrl ControllerInput::keyboardUrl(const QRect &field) {
    QUrl url("steam://open/keyboard");
    QUrlQuery query;
    query.addQueryItem("XPosition", QString::number(qMax(0, field.x())));
    query.addQueryItem("YPosition", QString::number(qMax(0, field.y())));
    query.addQueryItem("Width", QString::number(qMax(0, field.width())));
    query.addQueryItem("Height", QString::number(qMax(0, field.height())));
    query.addQueryItem("Mode", "0"); // Single line; Steam sends ordinary keyboard events to Qt.
    url.setQuery(query);
    return url;
}
bool ControllerInput::showKeyboard(int x, int y, int width, int height) {
    return QDesktopServices::openUrl(keyboardUrl(QRect(x, y, width, height)));
}
void ControllerInput::refresh() {
    if (m_controller && SDL_GameControllerGetAttached(m_controller)) return;
    if (m_controller) SDL_GameControllerClose(m_controller);
    m_controller = nullptr; m_held.clear(); m_axisX = m_axisY = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) { m_controller = SDL_GameControllerOpen(i); if (m_controller) break; }
    }
    m_name = m_controller ? QString::fromUtf8(SDL_GameControllerName(m_controller)) : "Keyboard";
    emit changed();
}
void ControllerInput::press(int key, bool repeat) {
    auto *window = QGuiApplication::focusWindow();
    if (!window || !window->isActive()) return;
    const auto *focus = window->focusObject();
    if (key == Qt::Key_Return && focus && focus->property("cursorPosition").isValid() && focus->property("echoMode").isValid()) {
        emit keyboardRequested();
        return;
    }
    QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier, {}, repeat);
    QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &down);
    QCoreApplication::sendEvent(window, &up);
}
void ControllerInput::poll() {
    const qint64 now = m_clock.elapsed();
    const auto held = [this, now](int key, bool down) {
        if (down && !m_held.contains(key)) { m_held[key] = now + 350; press(key); }
        if (!down) m_held.remove(key);
    };
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED) { refresh(); continue; }
        if (!m_controller) continue;
        const auto id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller));
        if ((e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) && e.cbutton.which == id) {
            int key = 0;
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A: key = Qt::Key_Return; break;
            case SDL_CONTROLLER_BUTTON_B: key = Qt::Key_Escape; break;
            case SDL_CONTROLLER_BUTTON_X: key = Qt::Key_Space; break;
            case SDL_CONTROLLER_BUTTON_Y: key = Qt::Key_F; break;
            case SDL_CONTROLLER_BUTTON_START: key = Qt::Key_F1; break;
            case SDL_CONTROLLER_BUTTON_BACK: key = Qt::Key_I; break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: key = Qt::Key_PageUp; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: key = Qt::Key_PageDown; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: key = Qt::Key_Up; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key = Qt::Key_Down; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = Qt::Key_Left; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = Qt::Key_Right; break;
            default: break;
            }
            if (key) held(key, e.type == SDL_CONTROLLERBUTTONDOWN);
        }
        if (e.type == SDL_CONTROLLERAXISMOTION && e.caxis.which == id && e.caxis.axis <= SDL_CONTROLLER_AXIS_LEFTY) {
            int &old = e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ? m_axisX : m_axisY;
            const int direction = e.caxis.value < -16000 ? -1 : e.caxis.value > 16000 ? 1 : qAbs(e.caxis.value) < 10000 ? 0 : old;
            if (old != direction) {
                const int negative = e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ? Qt::Key_Left : Qt::Key_Up;
                const int positive = e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ? Qt::Key_Right : Qt::Key_Down;
                held(negative, direction == -1); held(positive, direction == 1); old = direction;
            }
        }
    }
    for (auto it = m_held.begin(); it != m_held.end(); ++it) {
        if (now >= it.value() && it.key() >= Qt::Key_Left && it.key() <= Qt::Key_Down) { press(it.key(), true); it.value() = now + 95; }
    }
}
