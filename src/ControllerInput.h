#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QRect>
#include <QUrl>
#include <SDL.h>

class ControllerInput : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY changed)
public:
    explicit ControllerInput(QObject *parent = nullptr);
    ~ControllerInput() override;
    QString name() const { return m_name; }
    Q_INVOKABLE bool showKeyboard(int x, int y, int width, int height);
    static QUrl keyboardUrl(const QRect &field);
signals:
    void changed();
    void keyboardRequested();
private:
    void poll();
    void refresh();
    void press(int key, bool repeat = false);
    QTimer m_timer;
    QElapsedTimer m_clock;
    QHash<int, qint64> m_held;
    SDL_GameController *m_controller = nullptr;
    QString m_name = "Keyboard";
    int m_axisX = 0, m_axisY = 0;
};
