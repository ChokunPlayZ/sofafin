#pragma once
#include <QObject>
#include <QSettings>

class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY changed)
    Q_PROPERTY(bool hardwareDecoding READ hardwareDecoding WRITE setHardwareDecoding NOTIFY changed)
    Q_PROPERTY(bool themeSongs READ themeSongs WRITE setThemeSongs NOTIFY changed)
    Q_PROPERTY(int themeSongVolume READ themeSongVolume WRITE setThemeSongVolume NOTIFY changed)
    Q_PROPERTY(double uiScale READ uiScale WRITE setUiScale NOTIFY changed)
    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY changed)
    Q_PROPERTY(QString playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY changed)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY changed)
public:
    explicit Settings(QObject *parent = nullptr);
    bool fullscreen() const { return m_store.value("fullscreen", true).toBool(); }
    bool hardwareDecoding() const { return m_store.value("hardwareDecoding", true).toBool(); }
    bool themeSongs() const { return m_store.value("themeSongs", true).toBool(); }
    int themeSongVolume() const { return qBound(0, m_store.value("themeSongVolume", 70).toInt(), 100); }
    double uiScale() const { return m_store.value("uiScale", 1.0).toDouble(); }
    int bitrate() const { return m_store.value("bitrate", 120000000).toInt(); }
    QString playbackMode() const { return m_store.value("playbackMode", "auto").toString(); }
    QString accent() const { return m_store.value("accent", "#66c0f4").toString(); }
    void setFullscreen(bool v) { put("fullscreen", v); }
    void setHardwareDecoding(bool v) { put("hardwareDecoding", v); }
    void setThemeSongs(bool v) { put("themeSongs", v); }
    void setThemeSongVolume(int v) { put("themeSongVolume", qBound(0, v, 100)); }
    void setUiScale(double v);
    void setBitrate(int v);
    void setPlaybackMode(const QString &v);
    void setAccent(const QString &v);
    QVariant value(const QString &key, const QVariant &fallback = {}) const { return m_store.value(key, fallback); }
    void put(const QString &key, const QVariant &value);
    void clearSession();
signals:
    void changed();
private:
    QSettings m_store;
};
