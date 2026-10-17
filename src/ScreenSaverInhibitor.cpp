#include "ScreenSaverInhibitor.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPointer>

namespace {
const QString service = QStringLiteral("org.freedesktop.ScreenSaver");
const QString path = QStringLiteral("/org/freedesktop/ScreenSaver");

void uninhibit(const QString &owner, quint32 cookie) {
    // Address the original service instance: a restarted service can reuse cookies.
    auto message = QDBusMessage::createMethodCall(owner, path, service, "UnInhibit");
    message.setAutoStartService(false);
    message << cookie;
    QDBusConnection::sessionBus().send(message);
}
}

ScreenSaverInhibitor::ScreenSaverInhibitor(QObject *parent)
    : QObject(parent), m_service(service, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange) {
    connect(&m_service, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] {
        ++m_generation;
        m_pending = false;
        release();
        if (m_inhibited) acquire();
    });
    m_retry.setInterval(30000);
    connect(&m_retry, &QTimer::timeout, this, &ScreenSaverInhibitor::acquire);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { setInhibited(false); });
}

ScreenSaverInhibitor::~ScreenSaverInhibitor() { release(); }

void ScreenSaverInhibitor::setInhibited(bool inhibited) {
    if (m_inhibited == inhibited) return;
    m_inhibited = inhibited;
    if (inhibited) {
        m_retry.start();
        acquire();
    } else {
        m_retry.stop();
        release();
    }
}

void ScreenSaverInhibitor::acquire() {
    if (!m_inhibited || m_cookie || m_pending) return;
    m_pending = true;
    const auto generation = m_generation;
    // Resolve the unique owner before requesting a cookie. Reply messages do not
    // always carry a service address (notably Qt's in-process D-Bus delivery).
    auto message = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner");
    message << service;
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        call->deleteLater();
        if (m_generation != generation) return;
        if (!m_inhibited || reply.isError()) { m_pending = false; return; }
        inhibit(reply.value());
    });
}

void ScreenSaverInhibitor::inhibit(const QString &owner) {
    auto message = QDBusMessage::createMethodCall(owner, path, service, "Inhibit");
    message << QStringLiteral("Sofafin") << tr("Playing video");
    const auto generation = m_generation;
    const QPointer<ScreenSaverInhibitor> self(this);
    // Let pending replies outlive the player so a late cookie can still be released.
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), QCoreApplication::instance());
    connect(call, &QDBusPendingCallWatcher::finished, call, [self, generation, owner](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<quint32> reply = *call;
        call->deleteLater();
        const bool current = self && self->m_generation == generation;
        if (current) self->m_pending = false;
        if (reply.isError()) return; // An unavailable service must not interrupt playback.
        if (current && self->m_inhibited) {
            self->m_owner = owner;
            self->m_cookie = reply.value();
        } else {
            uninhibit(owner, reply.value());
        }
    });
}

void ScreenSaverInhibitor::release() {
    if (!m_cookie) return;
    uninhibit(m_owner, *m_cookie);
    m_cookie.reset();
    m_owner.clear();
}
