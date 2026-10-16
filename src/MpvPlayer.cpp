#include "MpvPlayer.h"
#include <QMetaObject>
#include <stdexcept>
#include <vector>

MpvPlayer::MpvPlayer(QObject *parent) : QObject(parent), m_core(std::make_shared<MpvCore>()) {
    auto *h = m_core->handle = mpv_create();
    if (!h) throw std::runtime_error("Could not create libmpv.");
    for (const auto &[key, value] : {std::pair{"vo", "libmpv"}, {"terminal", "no"}, {"config", "no"},
         {"input-default-bindings", "no"}, {"input-vo-keyboard", "no"}, {"osc", "no"},
         {"idle", "yes"}, {"keep-open", "no"}, {"hwdec", "auto-safe"}, {"audio-channels", "stereo"}})
        mpv_set_option_string(h, key, value);
    if (mpv_initialize(h) < 0) throw std::runtime_error("Could not initialize libmpv.");
    mpv_observe_property(h, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(h, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(h, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(h, 0, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(h, 0, "track-list", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "aid", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "sid", MPV_FORMAT_NONE);
    mpv_set_wakeup_callback(h, [](void *ctx) {
        auto *self = static_cast<MpvPlayer *>(ctx);
        QMetaObject::invokeMethod(self, [self] { self->drainEvents(); }, Qt::QueuedConnection);
    }, this);
}
MpvPlayer::~MpvPlayer() { mpv_set_wakeup_callback(m_core->handle, nullptr, nullptr); }
void MpvPlayer::command(std::initializer_list<QByteArray> args) {
    std::vector<const char *> ptrs;
    for (const auto &s : args) ptrs.push_back(s.constData());
    ptrs.push_back(nullptr);
    mpv_command_async(m_core->handle, 0, ptrs.data());
}
void MpvPlayer::load(const QUrl &url, double start, double offset, double duration, bool hwdec) {
    m_pendingUrl = url; m_start = start; m_offset = offset;
    m_position = 0; m_duration = duration; m_active = true; m_loading = true; m_paused = false;
    mpv_set_property_string(m_core->handle, "hwdec", hwdec ? "auto-safe" : "no");
    mpv_set_property_string(m_core->handle, "sub-delay", QByteArray::number(-offset, 'f', 3).constData());
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
    m_pendingUrl = QUrl(); m_active = false; m_loading = false;
    command({"stop"}); emit changed();
}
void MpvPlayer::togglePause() { command({"cycle", "pause"}); }
void MpvPlayer::seek(double seconds) { command({"seek", QByteArray::number(qMax(0.0, seconds - m_offset), 'f', 3), "absolute+exact"}); }
void MpvPlayer::cycleAudio() { command({"cycle", "aid"}); }
void MpvPlayer::cycleSubtitle() { command({"cycle", "sid"}); }
void MpvPlayer::addSubtitle(const QUrl &url) { command({"sub-add", url.toEncoded(), "auto"}); }
void MpvPlayer::updateTrackNames() {
    auto *h = m_core->handle;
    mpv_node tracks{};
    if (mpv_get_property(h, "track-list", MPV_FORMAT_NODE, &tracks) < 0) return;
    m_audio = "Auto"; m_subtitle = "Off";
    if (tracks.format == MPV_FORMAT_NODE_ARRAY) for (int i = 0; i < tracks.u.list->num; ++i) {
        const auto &track = tracks.u.list->values[i];
        if (track.format != MPV_FORMAT_NODE_MAP) continue;
        QString type, title, lang; bool selected = false;
        for (int j = 0; j < track.u.list->num; ++j) {
            const QByteArray key(track.u.list->keys[j]); const auto &v = track.u.list->values[j];
            if (key == "selected" && v.format == MPV_FORMAT_FLAG) selected = v.u.flag;
            if (v.format != MPV_FORMAT_STRING) continue;
            if (key == "type") type = QString::fromUtf8(v.u.string);
            if (key == "title") title = QString::fromUtf8(v.u.string);
            if (key == "lang") lang = QString::fromUtf8(v.u.string);
        }
        if (selected) {
            const auto label = !title.isEmpty() ? title : !lang.isEmpty() ? lang.toUpper() : "Default";
            if (type == "audio") m_audio = label;
            if (type == "sub") m_subtitle = label;
        }
    }
    mpv_free_node_contents(&tracks);
}
void MpvPlayer::drainEvents() {
    for (;;) {
        auto *event = mpv_wait_event(m_core->handle, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        if (event->event_id == MPV_EVENT_FILE_LOADED) { m_loading = false; emit loaded(); }
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *p = static_cast<mpv_event_property *>(event->data);
            const QByteArray name(p->name);
            if (p->data && p->format == MPV_FORMAT_DOUBLE) {
                if (name == "time-pos") m_position = *static_cast<double *>(p->data);
                if (name == "duration" && m_offset == 0) m_duration = *static_cast<double *>(p->data);
            }
            if (p->data && p->format == MPV_FORMAT_FLAG) {
                if (name == "pause") m_paused = *static_cast<int *>(p->data);
                if (name == "paused-for-cache") m_loading = *static_cast<int *>(p->data);
            }
            if (name == "track-list" || name == "aid" || name == "sid") updateTrackNames();
        }
        if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_EOF || end->reason == MPV_END_FILE_REASON_ERROR) {
                m_active = false; m_loading = false; emit ended();
                if (end->reason == MPV_END_FILE_REASON_ERROR) emit error("mpv could not play this stream. Check the connection or try transcoding in Settings.");
            }
        }
        emit changed();
    }
}
