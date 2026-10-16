#pragma once
#include <QObject>
#include <QUrl>
#include <memory>
#include <mpv/client.h>

struct MpvCore {
    mpv_handle *handle = nullptr;
    ~MpvCore() { if (handle) mpv_terminate_destroy(handle); }
};

class MpvPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(double position READ position NOTIFY changed)
    Q_PROPERTY(double duration READ duration NOTIFY changed)
    Q_PROPERTY(double bufferedPosition READ bufferedPosition NOTIFY changed)
    Q_PROPERTY(int bufferPercent READ bufferPercent NOTIFY changed)
    Q_PROPERTY(QString audioTrack READ audioTrack NOTIFY changed)
    Q_PROPERTY(QString subtitleTrack READ subtitleTrack NOTIFY changed)
public:
    explicit MpvPlayer(QObject *parent = nullptr, bool audioOnly = false);
    ~MpvPlayer() override;
    std::shared_ptr<MpvCore> core() const { return m_core; }
    bool active() const { return m_active; }
    bool paused() const { return m_paused; }
    bool loading() const { return m_loading; }
    double position() const { return m_position + m_offset; }
    double duration() const { return m_duration; }
    double bufferedPosition() const { return qBound(position(), m_buffered + m_offset, qMax(position(), m_duration)); }
    int bufferPercent() const { return m_bufferPercent; }
    QString audioTrack() const { return m_audio; }
    QString subtitleTrack() const { return m_subtitle; }
    void load(const QUrl &url, double start, double offset, double duration, bool hwdec);
    void addSubtitle(const QUrl &url, bool select = false);
    void selectStream(int index, bool subtitle);
    void setPaused(bool paused);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void cycleAudio();
    Q_INVOKABLE void cycleSubtitle();
    void rendererReady();
    void rendererFailed();
signals:
    void changed();
    void loaded();
    void ended();
    void error(const QString &message);
private:
    void drainEvents();
    void loadPending();
    void command(std::initializer_list<QByteArray> args);
    void updateTrackNames();
    std::shared_ptr<MpvCore> m_core;
    QUrl m_pendingUrl;
    double m_start = 0, m_position = 0, m_offset = 0, m_duration = 0, m_buffered = 0;
    int m_bufferPercent = 0;
    bool m_fileLoading = false, m_cacheLoading = false;
    bool m_active = false, m_paused = false, m_loading = false, m_ready = false;
    QString m_audio = "Auto", m_subtitle = "Off";
};
