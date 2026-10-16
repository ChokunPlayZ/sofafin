#include "Playback.h"
#include <QJsonArray>
#include <QUrlQuery>

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
    const QString &deviceId, const QString &token, const QString &mode, qint64 startTicks) {
    Plan p;
    p.sessionId = response.value("PlaySessionId").toString();
    if (!response.value("ErrorCode").toString().isEmpty()) {
        p.error = "The server could not prepare this title: " + response.value("ErrorCode").toString();
        return p;
    }
    const auto sources = response.value("MediaSources").toArray();
    // Prefer any directly playable version before considering server transcoding.
    for (const auto &s : sources) {
        if (mode != "transcode" && s.toObject().value("SupportsDirectPlay").toBool()) { p.source = s.toObject(); break; }
    }
    if (p.source.isEmpty() && mode != "direct") {
        for (const auto &s : sources) {
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
        p.url = relative.isRelative() ? server.resolved(relative) : relative;
        // Jellyfin's root-relative URLs normally include its configured base path.
        if (relative.isRelative() && !relative.path().startsWith('/')) {
            p.url = endpoint(server, relative.path());
            p.url.setQuery(relative.query());
        }
        p.offset = double(startTicks) / 10000000.0;
    }
    if (!sameOrigin(p.url, server)) { p.url = {}; p.error = "The server returned a playback URL on a different origin."; return p; }
    QUrlQuery q(p.url);
    q.removeAllQueryItems("api_key");
    q.addQueryItem("api_key", token);
    p.url.setQuery(q);
    return p;
}
