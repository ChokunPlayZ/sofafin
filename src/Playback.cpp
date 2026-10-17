#include "Playback.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

QUrl Playback::endpoint(const QUrl &server, const QString &path) {
    QUrl result(server);
    QString base = server.path();
    while (base.endsWith('/')) base.chop(1);
    result.setPath(base + "/" + (path.startsWith('/') ? path.mid(1) : path));
    result.setQuery(QString());
    result.setFragment(QString());
    return result;
}
bool Playback::sameOrigin(const QUrl &a, const QUrl &b) {
    auto port = [](const QUrl &u) { return u.port(u.scheme() == "https" ? 443 : 80); };
    return a.scheme() == b.scheme() && a.host() == b.host() && port(a) == port(b) && a.userInfo().isEmpty();
}
QUrl Playback::resourceUrl(const QUrl &server, const QUrl &resource) {
    if (!resource.isRelative()) return resource;
    QString base = server.path();
    while (base.endsWith('/')) base.chop(1);
    auto result = !base.isEmpty() && (resource.path() == base || resource.path().startsWith(base + '/'))
        ? server.resolved(resource) : endpoint(server, resource.path());
    result.setQuery(resource.query());
    result.setFragment(resource.fragment());
    return result;
}
QJsonObject Playback::deviceProfile(int bitrate) {
    return {{"Name", "JF Gamescope / libmpv"}, {"MaxStreamingBitrate", bitrate}, {"MaxStaticBitrate", 1000000000},
        {"DirectPlayProfiles", QJsonArray{QJsonObject{{"Type", "Video"},
            {"Container", "mkv,mp4,m4v,mov,avi,ts,mpegts,m2ts,webm,mpg,mpeg,ogv"},
            {"VideoCodec", "h264,hevc,av1,vp8,vp9,mpeg2video,mpeg4,vc1,theora"},
            {"AudioCodec", "aac,ac3,eac3,dts,truehd,flac,mp3,opus,vorbis,pcm_s16le,pcm_s24le"}}}},
        {"TranscodingProfiles", QJsonArray{QJsonObject{{"Type", "Video"}, {"Context", "Streaming"},
            {"Protocol", "hls"}, {"Container", "ts"}, {"VideoCodec", "h264"}, {"AudioCodec", "aac"},
            {"MaxAudioChannels", "2"}, {"MinSegments", 2}, {"BreakOnNonKeyFrames", true}}}},
        {"SubtitleProfiles", QJsonArray{QJsonObject{{"Format", "srt"}, {"Method", "External"}},
            QJsonObject{{"Format", "ass"}, {"Method", "External"}}, QJsonObject{{"Format", "ssa"}, {"Method", "External"}},
            QJsonObject{{"Format", "subrip"}, {"Method", "External"}}, QJsonObject{{"Format", "vtt"}, {"Method", "External"}},
            QJsonObject{{"Format", "pgssub"}, {"Method", "Embed"}}, QJsonObject{{"Format", "dvdsub"}, {"Method", "Embed"}}}}};
}
QJsonObject Playback::request(const QString &userId, const QString &mode, int bitrate, qint64 startTicks) {
    return {{"UserId", userId}, {"DeviceProfile", deviceProfile(bitrate)}, {"MaxStreamingBitrate", bitrate},
        {"StartTimeTicks", startTicks}, {"IsPlayback", true}, {"AutoOpenLiveStream", false},
        {"EnableDirectPlay", mode != "transcode"}, {"EnableDirectStream", false},
        {"EnableTranscoding", mode != "direct"}, {"AllowVideoStreamCopy", mode != "transcode"},
        {"AllowAudioStreamCopy", mode != "transcode"}};
}
Playback::Plan Playback::negotiate(const QJsonObject &response, const QUrl &server, const QString &itemId,
    const QString &deviceId, const QString &token, const QString &mode, qint64 startTicks, const QString &mediaSourceId) {
    Plan p;
    p.sessionId = response.value("PlaySessionId").toString();
    if (!response.value("ErrorCode").toString().isEmpty()) {
        p.error = "The server could not prepare this title: " + response.value("ErrorCode").toString();
        return p;
    }
    const auto sources = response.value("MediaSources").toArray();
    // Prefer any directly playable version before considering server transcoding.
    for (const auto &s : sources) {
        if (!mediaSourceId.isEmpty() && s.toObject().value("Id").toString() != mediaSourceId) continue;
        if (mode != "transcode" && s.toObject().value("SupportsDirectPlay").toBool()) { p.source = s.toObject(); break; }
    }
    if (p.source.isEmpty() && mode != "direct") {
        for (const auto &s : sources) {
            if (!mediaSourceId.isEmpty() && s.toObject().value("Id").toString() != mediaSourceId) continue;
            if (!s.toObject().value("TranscodingUrl").toString().isEmpty()) { p.source = s.toObject(); break; }
        }
    }
    if (p.source.isEmpty()) { p.error = mode == "direct" ? "Direct play is unavailable. Choose Automatic in Settings to allow transcoding." : "No playable media source was returned by the server."; return p; }
    if (p.source.value("RequiresOpening").toBool()) { p.error = "Live streams are not supported yet."; return p; }
    p.mediaSourceId = p.source.value("Id").toString();
    if (mode != "transcode" && p.source.value("SupportsDirectPlay").toBool()) {
        p.method = "DirectPlay";
        p.url = endpoint(server, "Videos/" + itemId + "/stream");
        QUrlQuery q;
        q.addQueryItem("Static", "true");
        q.addQueryItem("MediaSourceId", p.mediaSourceId);
        q.addQueryItem("DeviceId", deviceId);
        q.addQueryItem("PlaySessionId", p.sessionId);
        p.url.setQuery(q);
    } else {
        p.method = "Transcode";
        const QUrl relative(p.source.value("TranscodingUrl").toString());
        p.url = resourceUrl(server, relative);
        p.offset = double(startTicks) / 10000000.0;
    }
    if (!sameOrigin(p.url, server)) { p.url = QUrl(); p.error = "The server returned a playback URL on a different origin."; return p; }
    QUrlQuery q(p.url);
    q.removeAllQueryItems("api_key");
    q.addQueryItem("api_key", token);
    p.url.setQuery(q);
    return p;
}

QVariantList Playback::tracks(const QJsonObject &source, const QString &type) {
    QVariantList result;
    for (const auto &value : source.value("MediaStreams").toArray()) {
        const auto stream = value.toObject();
        if (stream.value("Type").toString() != type || !stream.contains("Index")) continue;
        QString label = stream.value("DisplayTitle").toString();
        if (label.isEmpty()) {
            QStringList parts;
            for (const auto &key : {"Language", "Codec", "Title"}) {
                const auto part = stream.value(key).toString();
                if (!part.isEmpty()) parts.append(part);
            }
            label = parts.isEmpty() ? type + " " + QString::number(stream.value("Index").toInt()) : parts.join(" · ");
        }
        result.append(QVariantMap{{"index", stream.value("Index").toInt()}, {"label", label}});
    }
    return result;
}
QString Playback::mediaInfo(const QJsonObject &source) {
    QStringList lines;
    QStringList format;
    const auto container = source.value("Container").toString().toUpper();
    if (!container.isEmpty()) format.append(container);
    if (source.value("Bitrate").toDouble() > 0) format.append(QString::number(source.value("Bitrate").toDouble() / 1000000, 'f', 1) + " Mbps");
    if (source.value("Size").toDouble() > 0) format.append(QString::number(source.value("Size").toDouble() / 1073741824, 'f', 2) + " GiB");
    if (!format.isEmpty()) lines.append(format.join(" · "));
    for (const auto &value : source.value("MediaStreams").toArray()) {
        const auto stream = value.toObject();
        const auto type = stream.value("Type").toString();
        if (type != "Video" && type != "Audio" && type != "Subtitle") continue;
        QString label = stream.value("DisplayTitle").toString();
        if (label.isEmpty()) {
            QStringList parts;
            const auto codec = stream.value("Codec").toString().toUpper();
            if (!codec.isEmpty()) parts.append(codec);
            if (stream.value("Width").toInt() > 0 && stream.value("Height").toInt() > 0)
                parts.append(QString("%1×%2").arg(stream.value("Width").toInt()).arg(stream.value("Height").toInt()));
            if (stream.value("Channels").toInt() > 0) parts.append(QString::number(stream.value("Channels").toInt()) + " ch");
            const auto language = stream.value("Language").toString();
            if (!language.isEmpty()) parts.append(language);
            label = parts.join(" · ");
        }
        if (!label.isEmpty()) lines.append(type + ": " + label);
    }
    return lines.join('\n');
}
QVariantList Playback::segments(const QJsonObject &response) {
    QVariantList result;
    for (const auto &value : response.value("Items").toArray()) {
        const auto segment = value.toObject();
        const double start = segment.value("StartTicks").toDouble(-1) / 10000000;
        const double end = segment.value("EndTicks").toDouble(-1) / 10000000;
        const auto type = segment.value("Type").toString();
        if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start ||
            !QStringList{"Intro", "Outro", "Recap", "Preview", "Commercial"}.contains(type)) continue;
        result.append(QVariantMap{{"start", start}, {"end", end}, {"type", type}});
    }
    std::stable_sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("start").toDouble() < b.toMap().value("start").toDouble();
    });
    return result;
}
QVariantList Playback::chapterSegments(const QVariantList &chapters, double duration) {
    if (!std::isfinite(duration) || duration <= 0) return {};
    QVariantList ordered;
    for (const auto &value : chapters) {
        bool valid = false;
        const double ticks = value.toMap().value("ticks").toDouble(&valid);
        if (valid && std::isfinite(ticks) && ticks >= 0 && ticks / 10000000 < duration) ordered.append(value);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("ticks").toDouble() < b.toMap().value("ticks").toDouble();
    });
    static const QList<QPair<QRegularExpression, QString>> types{
        {QRegularExpression("^(intro|opening|opening credits|op)(?:\\s*\\d+)?$"), "Intro"},
        {QRegularExpression("^(outro|ending|closing credits|end credits|ending credits|credits|ed)(?:\\s*\\d+)?$"), "Outro"},
        {QRegularExpression("^(recap|recapitulation|previously|previously on)$"), "Recap"},
        {QRegularExpression("^(preview|episode preview|next episode|next time)$"), "Preview"},
        {QRegularExpression("^(commercials?|advertisements?|ads)$"), "Commercial"}};
    QVariantList result;
    for (qsizetype i = 0; i < ordered.size(); ++i) {
        const auto chapter = ordered[i].toMap();
        const auto name = chapter.value("name").toString().simplified().toCaseFolded();
        QString type;
        for (const auto &entry : types) {
            if (entry.first.match(name).hasMatch()) { type = entry.second; break; }
        }
        if (type.isEmpty()) continue;
        const double start = chapter.value("ticks").toDouble() / 10000000;
        double end = duration;
        for (qsizetype j = i + 1; j < ordered.size(); ++j) {
            const double next = ordered[j].toMap().value("ticks").toDouble() / 10000000;
            if (next > start) { end = next; break; }
        }
        result.append(QVariantMap{{"start", start}, {"end", end}, {"type", type}, {"source", "chapters"}});
    }
    return result;
}
QVariantMap Playback::activeSegment(const QVariantList &segments, double position) {
    for (const auto &value : segments) {
        const auto segment = value.toMap();
        if (position >= segment.value("start").toDouble() && position < segment.value("end").toDouble()) return segment;
    }
    return {};
}
