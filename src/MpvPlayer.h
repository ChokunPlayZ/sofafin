#pragma once
#include <QObject>
#include <QUrl>
#include <memory>
#include <QHash>
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
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY changed)
    Q_PROPERTY(double position READ position NOTIFY changed)
    Q_PROPERTY(double duration READ duration NOTIFY changed)
    Q_PROPERTY(double bufferedPosition READ bufferedPosition NOTIFY changed)
    Q_PROPERTY(int bufferPercent READ bufferPercent NOTIFY changed)
    Q_PROPERTY(double networkSpeed READ networkSpeed NOTIFY changed)
    Q_PROPERTY(QString audioTrack READ audioTrack NOTIFY changed)
    Q_PROPERTY(QString subtitleTrack READ subtitleTrack NOTIFY changed)
public:
    explicit MpvPlayer(QObject *parent = nullptr, bool audioOnly = false);
    ~MpvPlayer() override;
    std::shared_ptr<MpvCore> core() const { return m_core; }
    bool active() const { return m_active; }
    bool paused() const { return m_paused; }
    bool loading() const { return m_active && (m_fileLoading || m_cacheLoading || m_seekLoading); }
    bool hasFrame() const { return m_hasFrame; }
    double position() const { return m_position + m_offset; }
    double duration() const { return m_duration; }
    double bufferedPosition() const { return qBound(position(), m_buffered + m_offset, qMax(position(), m_duration)); }
    int bufferPercent() const { return m_bufferPercent; }
    double networkSpeed() const { return m_active && m_networkStream ? m_networkSpeed : -1; }
    QString audioTrack() const { return m_audio; }
    QString subtitleTrack() const { return m_subtitle; }
    void load(const QUrl &url, double start, double offset, double duration, bool hwdec);
    void addSubtitle(const QUrl &url, bool select = false);
    void selectStream(int index, bool subtitle);
    void setPaused(bool paused);
    void setVolume(int volume);
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
    int command(std::initializer_list<QByteArray> args, quint64 request = 0);
    void updateTracks(const mpv_node &tracks);
    void applyTrackSelections();
    std::shared_ptr<MpvCore> m_core;
    QUrl m_pendingUrl;
    double m_start = 0, m_position = 0, m_offset = 0, m_duration = 0, m_buffered = 0;
    int m_bufferPercent = 0;
    double m_networkSpeed = -1;
    bool m_fileLoading = false, m_cacheLoading = false, m_seekLoading = false;
    bool m_active = false, m_paused = false, m_ready = false, m_networkStream = false;
    bool m_hasFrame = false;
    quint64 m_seekGeneration = 0, m_pendingSeek = 0;
    QString m_audio = "Auto", m_subtitle = "Off";
    QHash<int, qint64> m_audioIds, m_subtitleIds;
    // -3 means no deferred selection; Automatic (-2) and Off (-1) apply immediately.
    int m_pendingAudio = -3, m_pendingSubtitle = -3;
    quint64 m_trackGeneration = 1;
};
