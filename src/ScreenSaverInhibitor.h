#pragma once
#include <QObject>
#include <QDBusServiceWatcher>
#include <QTimer>
#include <optional>

class ScreenSaverInhibitor : public QObject {
    Q_OBJECT
public:
    explicit ScreenSaverInhibitor(QObject *parent = nullptr);
    ~ScreenSaverInhibitor() override;
    void setInhibited(bool inhibited);
private:
    void acquire();
    void inhibit(const QString &owner);
    void release();
    QDBusServiceWatcher m_service;
    QTimer m_retry;
    QString m_owner;
    std::optional<quint32> m_cookie;
    quint64 m_generation = 0;
    bool m_inhibited = false, m_pending = false;
};
