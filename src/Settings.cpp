#include "Settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
#include <cmath>

Settings::Settings(QObject *parent) : QObject(parent), m_store(QSettings::IniFormat, QSettings::UserScope, "Sofafin", "client") {
    if (!QFile::exists(m_store.fileName())) {
        // Move the previous application's settings once, including its device ID and session.
        QSettings legacy(QSettings::IniFormat, QSettings::UserScope, "JF-Gamescope", "client");
        if (QFile::exists(legacy.fileName())) {
            if (QFileInfo(m_store.fileName()).absoluteDir().mkpath(".") && QFile::rename(legacy.fileName(), m_store.fileName())) {
                QFile::setPermissions(m_store.fileName(), QFile::ReadOwner | QFile::WriteOwner);
                m_store.sync();
            } else {
                qWarning("Sofafin: Could not migrate existing settings.");
            }
        }
    }
    if (!m_store.contains("deviceId")) put("deviceId", QUuid::createUuid().toString(QUuid::WithoutBraces));
}
void Settings::put(const QString &key, const QVariant &value) {
    if (m_store.value(key) == value) return;
    m_store.setValue(key, value);
    m_store.sync();
    QFile::setPermissions(m_store.fileName(), QFile::ReadOwner | QFile::WriteOwner);
    emit changed();
}
void Settings::setUiScale(double v) { if (std::isfinite(v)) put("uiScale", std::clamp(v, 0.75, 1.5)); }
void Settings::setBitrate(int v) { put("bitrate", std::clamp(v, 1000000, 1000000000)); }
void Settings::setPlaybackMode(const QString &v) {
    if (v == "auto" || v == "direct" || v == "transcode") put("playbackMode", v);
}
void Settings::setAccent(const QString &v) {
    if (QStringList{"#66c0f4", "#76dfba", "#f3bb70"}.contains(v)) put("accent", v);
}
void Settings::clearSession() {
    m_store.remove("accessToken");
    m_store.remove("userId");
    m_store.sync();
    emit changed();
}
