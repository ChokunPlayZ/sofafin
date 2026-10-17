#include "MpvPlayer.h"
#include <QMetaObject>
#include <stdexcept>
#include <vector>

MpvPlayer::MpvPlayer(QObject *parent, bool audioOnly) : QObject(parent), m_core(std::make_shared<MpvCore>()) {
    auto *h = m_core->handle = mpv_create();
    if (!h) throw std::runtime_error("Could not create libmpv.");
    for (const auto &[key, value] : {std::pair{"vo", "libmpv"}, {"terminal", "no"}, {"config", "no"},
         {"input-default-bindings", "no"}, {"input-vo-keyboard", "no"}, {"osc", "no"},
         {"idle", "yes"}, {"keep-open", "no"}, {"hwdec", "no"}, {"audio-channels", "stereo"}})
        mpv_set_option_string(h, key, value);
    if (audioOnly) {
        mpv_set_option_string(h, "vo", "null");
        mpv_set_option_string(h, "vid", "no");
        mpv_set_option_string(h, "loop-file", "inf");
        m_ready = true;
    } else {
        // Keep downloading continuously, with room for large UHD streams and
        // cached backward seeks. These are ceilings, not upfront allocations.
        for (const auto &[key, value] : {std::pair{"cache", "yes"}, {"cache-secs", "3600"},
             {"demuxer-max-bytes", "1GiB"}, {"demuxer-max-back-bytes", "256MiB"},
             {"demuxer-hysteresis-secs", "0"}, {"cache-pause", "yes"},
             {"cache-pause-initial", "yes"}, {"cache-pause-wait", "5"}})
            mpv_set_option_string(h, key, value);
    }
    if (mpv_initialize(h) < 0) throw std::runtime_error("Could not initialize libmpv.");
    mpv_observe_property(h, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(h, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(h, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(h, 0, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(h, 0, "demuxer-cache-time", MPV_FORMAT_DOUBLE);
    mpv_observe_property(h, 0, "cache-buffering-state", MPV_FORMAT_INT64);
    mpv_observe_property(h, 0, "cache-speed", MPV_FORMAT_INT64);
    mpv_observe_property(h, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(h, 0, "aid", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "sid", MPV_FORMAT_NONE);
    mpv_set_wakeup_callback(h, [](void *ctx) {
        auto *self = static_cast<MpvPlayer *>(ctx);
        QMetaObject::invokeMethod(self, [self] { self->drainEvents(); }, Qt::QueuedConnection);
    }, this);
}
MpvPlayer::~MpvPlayer() { mpv_set_wakeup_callback(m_core->handle, nullptr, nullptr); }
int MpvPlayer::command(std::initializer_list<QByteArray> args, quint64 request) {
    std::vector<const char *> ptrs;
    for (const auto &s : args) ptrs.push_back(s.constData());
    ptrs.push_back(nullptr);
    return mpv_command_async(m_core->handle, request, ptrs.data());
}
void MpvPlayer::load(const QUrl &url, double start, double offset, double duration, bool hwdec) {
    ++m_trackGeneration; m_audioIds.clear(); m_subtitleIds.clear();
    m_pendingAudio = m_pendingSubtitle = -3; m_audio = "Auto"; m_subtitle = "Off";
    m_pendingUrl = url; m_start = start; m_offset = offset;
    m_position = start; m_duration = duration; m_active = true; m_paused = false; m_hasFrame = false;
    m_fileLoading = m_seekLoading = true; m_cacheLoading = false; m_pendingSeek = 0;
    m_buffered = 0; m_bufferPercent = 0; m_networkSpeed = -1;
    m_networkStream = url.scheme() == "http" || url.scheme() == "https";
#ifdef Q_OS_LINUX
    // auto/auto-safe can probe FFmpeg's Vulkan decoder, whose device creation
    // aborts inside Gamescope's injected WSI layer. Our renderer uses OpenGL.
    // Restrict hardware decoding to VA-API/NVDEC; mpv falls back to software.
    const char *decoders = hwdec ? "vaapi,vaapi-copy,nvdec,nvdec-copy" : "no";
    if (mpv_set_property_string(m_core->handle, "hwdec", decoders) < 0)
        // Older mpv releases accept a single decoder rather than a list.
        mpv_set_property_string(m_core->handle, "hwdec", hwdec ? "vaapi-copy" : "no");
#else
    mpv_set_property_string(m_core->handle, "hwdec", hwdec ? "auto-safe" : "no");
#endif
    mpv_set_property_string(m_core->handle, "sub-delay", QByteArray::number(-offset, 'f', 3).constData());
    mpv_set_property_string(m_core->handle, "aid", "auto");
    mpv_set_property_string(m_core->handle, "sid", "auto");
    emit changed();
    if (m_ready) loadPending();
}
void MpvPlayer::loadPending() {
    if (m_pendingUrl.isEmpty()) return;
    // Set the next file's start option separately: works across mpv's 0.38 loadfile argument change.
    mpv_set_property_string(m_core->handle, "options/start", QByteArray::number(m_start, 'f', 3).constData());
    command({"set", "pause", "no"});
    command({"loadfile", m_pendingUrl.toEncoded(), "replace"});
    m_pendingUrl = QUrl();
}
void MpvPlayer::rendererReady() { m_ready = true; loadPending(); }
void MpvPlayer::rendererFailed() { m_ready = false; stop(); emit error("Could not initialize the OpenGL video renderer."); }
void MpvPlayer::stop() {
    ++m_trackGeneration; m_audioIds.clear(); m_subtitleIds.clear(); m_pendingAudio = m_pendingSubtitle = -3;
    m_pendingUrl = QUrl(); m_active = m_hasFrame = false; m_pendingSeek = 0;
    m_fileLoading = m_cacheLoading = m_seekLoading = m_paused = false;
    m_start = m_position = m_offset = m_duration = 0; m_audio = "Auto"; m_subtitle = "Off";
    m_buffered = 0; m_bufferPercent = 0; m_networkSpeed = -1;
    command({"stop"}); emit changed();
}
void MpvPlayer::togglePause() { command({"cycle", "pause"}); }
void MpvPlayer::setVolume(int volume) {
    double level = qBound(0, volume, 100);
    mpv_set_property(m_core->handle, "volume", MPV_FORMAT_DOUBLE, &level);
}
void MpvPlayer::setPaused(bool paused) {
    if (mpv_set_property_string(m_core->handle, "pause", paused ? "yes" : "no") >= 0) {
        m_paused = paused; emit changed();
    }
}
void MpvPlayer::seek(double seconds) {
    if (!m_active) return;
    // Show feedback before mpv starts the network seek or decoder catch-up.
    m_seekLoading = true; m_bufferPercent = 0; m_pendingSeek = ++m_seekGeneration;
    if (command({"seek", QByteArray::number(qMax(0.0, seconds - m_offset), 'f', 3), "absolute+exact"}, m_pendingSeek) < 0) {
        m_pendingSeek = 0; m_seekLoading = false;
    }
    emit changed();
}
void MpvPlayer::cycleAudio() { command({"cycle", "aid"}); }
void MpvPlayer::cycleSubtitle() { command({"cycle", "sid"}); }
void MpvPlayer::addSubtitle(const QUrl &url, bool select) { command({"sub-add", url.toEncoded(), select ? "select" : "auto"}); }
void MpvPlayer::selectStream(int index, bool subtitle) {
    const QByteArray property = subtitle ? "sid" : "aid";
    (subtitle ? m_pendingSubtitle : m_pendingAudio) = -3;
    if (index < 0) { command({"set", property, index == -1 ? "no" : "auto"}); return; }
    (subtitle ? m_pendingSubtitle : m_pendingAudio) = index;
    applyTrackSelections();
    mpv_get_property_async(m_core->handle, m_trackGeneration, "track-list", MPV_FORMAT_NODE);
}
void MpvPlayer::applyTrackSelections() {
    if (m_fileLoading) return;
    const auto apply = [this](int &index, const QHash<int, qint64> &ids, const QByteArray &property) {
        if (index >= 0 && ids.contains(index)) { command({"set", property, QByteArray::number(ids.value(index))}); index = -3; }
    };
    apply(m_pendingAudio, m_audioIds, "aid"); apply(m_pendingSubtitle, m_subtitleIds, "sid");
}
void MpvPlayer::updateTracks(const mpv_node &tracks) {
    if (m_fileLoading || !m_active) return;
    m_audioIds.clear(); m_subtitleIds.clear();
    m_audio = "Auto"; m_subtitle = "Off";
    if (tracks.format == MPV_FORMAT_NODE_ARRAY) for (int i = 0; i < tracks.u.list->num; ++i) {
        const auto &track = tracks.u.list->values[i];
        if (track.format != MPV_FORMAT_NODE_MAP) continue;
        QString type, title, lang; bool selected = false, external = false;
        qint64 id = -1, streamIndex = -1;
        for (int j = 0; j < track.u.list->num; ++j) {
            const QByteArray key(track.u.list->keys[j]); const auto &v = track.u.list->values[j];
            if (key == "selected" && v.format == MPV_FORMAT_FLAG) selected = v.u.flag;
            if (key == "external" && v.format == MPV_FORMAT_FLAG) external = v.u.flag;
            if (key == "id" && v.format == MPV_FORMAT_INT64) id = v.u.int64;
            if (key == "ff-index" && v.format == MPV_FORMAT_INT64) streamIndex = v.u.int64;
            if (v.format != MPV_FORMAT_STRING) continue;
            if (key == "type") type = QString::fromUtf8(v.u.string);
            if (key == "title") title = QString::fromUtf8(v.u.string);
            if (key == "lang") lang = QString::fromUtf8(v.u.string);
        }
        if (!external && id >= 0 && streamIndex >= 0) {
            if (type == "audio") m_audioIds.insert(int(streamIndex), id);
            if (type == "sub") m_subtitleIds.insert(int(streamIndex), id);
        }
        if (selected) {
            const auto label = !title.isEmpty() ? title : !lang.isEmpty() ? lang.toUpper() : "Default";
            if (type == "audio") m_audio = label;
            if (type == "sub") m_subtitle = label;
        }
    }
    applyTrackSelections();
}
void MpvPlayer::drainEvents() {
    for (;;) {
        auto *event = mpv_wait_event(m_core->handle, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            m_fileLoading = false; emit loaded();
            mpv_get_property_async(m_core->handle, m_trackGeneration, "track-list", MPV_FORMAT_NODE);
        }
        if (event->event_id == MPV_EVENT_COMMAND_REPLY && m_pendingSeek && event->reply_userdata == m_pendingSeek) {
            m_pendingSeek = 0;
            if (event->error < 0) m_seekLoading = false;
        }
        if (event->event_id == MPV_EVENT_SEEK && m_active) {
            m_seekLoading = true; m_bufferPercent = 0;
        }
        if (event->event_id == MPV_EVENT_PLAYBACK_RESTART && !m_pendingSeek) {
            m_seekLoading = false;
            if (m_active && !m_fileLoading) m_hasFrame = true;
        }
        if (event->event_id == MPV_EVENT_GET_PROPERTY_REPLY && event->reply_userdata == m_trackGeneration && event->error >= 0) {
            const auto *p = static_cast<mpv_event_property *>(event->data);
            if (p && p->data && p->format == MPV_FORMAT_NODE) updateTracks(*static_cast<mpv_node *>(p->data));
        }
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *p = static_cast<mpv_event_property *>(event->data);
            const QByteArray name(p->name);
            if (m_active && p->data && p->format == MPV_FORMAT_DOUBLE) {
                if (name == "time-pos") m_position = *static_cast<double *>(p->data);
                if (name == "duration" && m_offset == 0) m_duration = *static_cast<double *>(p->data);
                if (name == "demuxer-cache-time") m_buffered = *static_cast<double *>(p->data);
            }
            if (name == "demuxer-cache-time" && (!m_active || !p->data)) m_buffered = 0;
            if (m_active && name == "cache-buffering-state" && p->data && p->format == MPV_FORMAT_INT64)
                m_bufferPercent = qBound(0, int(*static_cast<int64_t *>(p->data)), 100);
            if (name == "cache-speed")
                m_networkSpeed = m_active && p->data && p->format == MPV_FORMAT_INT64
                    ? qMax(int64_t(-1), *static_cast<int64_t *>(p->data)) : -1;
            if (p->data && p->format == MPV_FORMAT_FLAG) {
                if (name == "pause") m_paused = m_active && *static_cast<int *>(p->data);
                if (name == "paused-for-cache") m_cacheLoading = m_active && *static_cast<int *>(p->data);
            }
            if (name == "track-list" && p->data && p->format == MPV_FORMAT_NODE) updateTracks(*static_cast<mpv_node *>(p->data));
            if (name == "aid" || name == "sid") mpv_get_property_async(m_core->handle, m_trackGeneration, "track-list", MPV_FORMAT_NODE);
        }
        if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_EOF || end->reason == MPV_END_FILE_REASON_ERROR) {
                m_active = m_hasFrame = false; m_fileLoading = m_cacheLoading = m_seekLoading = false;
                m_pendingSeek = 0; m_networkSpeed = -1; emit ended();
                if (end->reason == MPV_END_FILE_REASON_ERROR) emit error("mpv could not play this stream. Check the connection or try transcoding in Settings.");
            }
        }
        emit changed();
    }
}
