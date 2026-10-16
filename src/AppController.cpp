#include "AppController.h"
#include <QJsonArray>
#include <QUrlQuery>

namespace {
const QString fields = "Overview,Genres,PrimaryImageAspectRatio,MediaSources";
QUrlQuery query(std::initializer_list<std::pair<QString, QString>> values) {
    QUrlQuery q;
    for (const auto &[key, value] : values) q.addQueryItem(key, value);
    return q;
}
}
AppController::AppController(Settings *settings, MpvPlayer *player, QObject *parent, QNetworkAccessManager *transport)
    : QObject(parent), m_settings(settings), m_player(player), m_api(settings->value("deviceId").toString(), this, transport) {
    connect(&m_api, &JellyfinClient::unauthorized, this, [this] { logout(); notify("Your session expired. Sign in again."); });
    connect(player, &MpvPlayer::error, this, [this](const QString &error) { finishSession(); setError(error); });
    connect(player, &MpvPlayer::ended, this, [this] { finishSession(); if (signedIn()) home(); });
    connect(player, &MpvPlayer::loaded, this, [this] {
        if (m_plan.sessionId.isEmpty()) return;
        m_reportedStart = true; report("Playing"); m_progress.start();
        for (const auto &value : m_plan.source.value("MediaStreams").toArray()) {
            const auto stream = value.toObject();
            if (stream.value("Type").toString() != "Subtitle" || !stream.value("IsExternal").toBool()) continue;
            const QString delivery = stream.value("DeliveryUrl").toString();
            if (delivery.isEmpty()) continue;
            auto url = Playback::resourceUrl(m_api.server(), QUrl(delivery));
            if (!Playback::sameOrigin(url, m_api.server())) continue;
            QUrlQuery q(url); q.removeAllQueryItems("api_key"); q.addQueryItem("api_key", m_api.token()); url.setQuery(q);
            m_player->addSubtitle(url);
        }
    });
    m_progress.setInterval(10000);
    connect(&m_progress, &QTimer::timeout, this, [this] { report("Playing/Progress"); });
}
void AppController::start() {
    const auto token = m_settings->value("accessToken").toString();
    const auto userId = m_settings->value("userId").toString();
    if (token.isEmpty() || userId.isEmpty()) return;
    m_api.configure(QUrl(savedServer()), token); m_pending = 1; emit changed();
    m_api.get("Users/Me", {}, [this, userId](const QJsonObject &user, const QString &error) {
        m_pending = 0;
        if (!error.isEmpty()) { setError(error); return; }
        m_userId = user.value("Id").toString(userId); m_username = user.value("Name").toString(savedUsername()); home();
    });
}
void AppController::login(const QString &server, const QString &username, const QString &password, bool remember) {
    if (busy()) return;
    QUrl url(server.trimmed());
    if ((url.scheme() != "http" && url.scheme() != "https") || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) {
        setError("Enter a full server address, such as http://192.168.1.10:8096 or https://media.example.com/jellyfin."); return;
    }
    if (username.trimmed().isEmpty()) { setError("Enter your Jellyfin username."); return; }
    m_api.configure(url); m_pending = 1; m_message.clear(); emit changed();
    m_api.post("Users/AuthenticateByName", {{"Username", username.trimmed()}, {"Pw", password}},
        [this, url, remember](const QJsonObject &result, const QString &error) {
        m_pending = 0;
        if (!error.isEmpty()) { setError(error); return; }
        const auto user = result.value("User").toObject(); const auto token = result.value("AccessToken").toString();
        if (token.isEmpty() || user.value("Id").toString().isEmpty()) { setError("The server did not return a valid session."); return; }
        m_api.configure(url, token); m_userId = user.value("Id").toString(); m_username = user.value("Name").toString();
        m_settings->put("server", url.toString()); m_settings->put("username", m_username);
        m_settings->clearSession();
        if (remember) { m_settings->put("accessToken", token); m_settings->put("userId", m_userId); }
        home();
    });
}
void AppController::logout() {
    stopPlayback();
    m_api.post("Sessions/Logout", {});
    ++m_generation; m_pending = 0; m_userId.clear(); m_username.clear(); m_shelves.clear(); m_libraries.clear(); m_history.clear();
    m_settings->clearSession(); m_api.configure({}); emit shelvesChanged(); emit librariesChanged(); emit changed();
}
void AppController::setError(const QString &error) { m_message = error; emit changed(); }
QVariantMap AppController::normalize(const QJsonObject &o) const {
    const auto data = o.value("UserData").toObject();
    const double ticks = o.value("RunTimeTicks").toDouble(), position = data.value("PlaybackPositionTicks").toDouble();
    const QString type = o.value("Type").toString();
    QString subtitle = QString::number(o.value("ProductionYear").toInt());
    if (subtitle == "0") subtitle = type;
    if (ticks > 0) subtitle += "  ·  " + QString::number(qRound(ticks / 600000000)) + " min";
    if (type == "Episode") subtitle = o.value("SeriesName").toString() + QString("  ·  S%1 E%2").arg(o.value("ParentIndexNumber").toInt()).arg(o.value("IndexNumber").toInt());
    QString id = o.value("Id").toString(), posterId = id;
    QString tag = o.value("ImageTags").toObject().value("Primary").toString();
    if (tag.isEmpty()) { posterId = o.value("SeriesId").toString(); tag = o.value("SeriesPrimaryImageTag").toString(); }
    auto backdrops = o.value("BackdropImageTags").toArray();
    QString backdropId = id, backdropTag;
    if (!backdrops.isEmpty()) backdropTag = backdrops.first().toString();
    else { backdropId = o.value("ParentBackdropItemId").toString(); const auto tags = o.value("ParentBackdropImageTags").toArray(); if (!tags.isEmpty()) backdropTag = tags.first().toString(); }
    return {{"id", id}, {"title", o.value("Name").toString()}, {"subtitle", subtitle}, {"type", type},
        {"overview", o.value("Overview").toString()}, {"runtime", ticks / 10000000}, {"positionTicks", position},
        {"progress", ticks > 0 ? position / ticks : 0}, {"poster", m_api.imageUrl(posterId, "Primary", tag, 400)},
        {"backdrop", m_api.imageUrl(backdropId, "Backdrop/0", backdropTag, 1280)},
        {"genres", o.value("Genres").toArray().toVariantList().mid(0, 3)},
        {"folder", o.value("IsFolder").toBool() || type == "Series" || type == "Season" || type == "CollectionFolder" || type == "BoxSet"}};
}
QVariantList AppController::items(const QJsonObject &response) const {
    QVariantList result;
    for (const auto &v : response.value("Items").toArray()) result.append(normalize(v.toObject()));
    return result;
}
void AppController::home(const QString &category) {
    if (!signedIn()) return;
    ++m_generation; m_history.clear(); m_pageParent.clear(); m_pageSearch.clear(); m_pageCategory = category;
    m_title = category; m_loaded = m_total = 0; m_shelves.clear(); m_message.clear();
    if (category != "Home") { fetchPage(); return; }
    m_pending = 4; const int generation = m_generation;
    const QStringList titles{"Continue watching", "Up next", "Recently added movies"};
    for (const auto &title : titles) m_shelves.append(QVariantMap{{"title", title}, {"items", QVariantList{}}});
    emit shelvesChanged();
    const auto done = [this, generation](int index, const QJsonObject &result, const QString &error) {
        if (generation != m_generation) return;
        if (index == -1) { m_libraries = items(result); emit librariesChanged(); }
        else { auto shelf = m_shelves[index].toMap(); shelf["items"] = items(result); m_shelves[index] = shelf; }
        --m_pending; if (!error.isEmpty()) m_message = error;
        if (m_pending == 0) emit shelvesChanged();
        emit changed(); if (m_pending == 0) emit browsingReady();
    };
    emit changed();
    m_api.get("Users/" + m_userId + "/Views", {}, [done](const auto &r, const auto &e) { done(-1, r, e); });
    m_api.get("Users/" + m_userId + "/Items/Resume", query({{"Limit", "20"}, {"Fields", fields}, {"MediaTypes", "Video"}}), [done](const auto &r, const auto &e) { done(0, r, e); });
    m_api.get("Shows/NextUp", query({{"UserId", m_userId}, {"Limit", "20"}, {"Fields", fields}}), [done](const auto &r, const auto &e) { done(1, r, e); });
    m_api.get("Users/" + m_userId + "/Items", query({{"Recursive", "true"}, {"IncludeItemTypes", "Movie"}, {"SortBy", "DateCreated"}, {"SortOrder", "Descending"}, {"Limit", "24"}, {"Fields", fields}}), [done](const auto &r, const auto &e) { done(2, r, e); });
}
void AppController::open(const QVariantMap &item) {
    if (!item.value("folder").toBool()) return;
    m_history.append({{"title", m_title}, {"parent", m_pageParent}, {"search", m_pageSearch}, {"category", m_pageCategory}});
    ++m_generation; m_title = item.value("title").toString(); m_pageParent = item.value("id").toString();
    m_pageSearch.clear(); m_pageCategory.clear(); m_loaded = m_total = 0; fetchPage();
}
void AppController::back() {
    if (m_history.isEmpty()) { home(); return; }
    auto previous = m_history.takeLast();
    if (previous.value("category") == "Home") { home(); return; }
    ++m_generation; m_title = previous.value("title").toString(); m_pageParent = previous.value("parent").toString();
    m_pageSearch = previous.value("search").toString(); m_pageCategory = previous.value("category").toString();
    m_loaded = m_total = 0; fetchPage();
}
void AppController::search(const QString &text) {
    if (text.trimmed().isEmpty() || !signedIn()) return;
    ++m_generation; m_history.clear(); m_pageParent.clear(); m_pageCategory.clear(); m_pageSearch = text.trimmed();
    m_title = "Results for “" + m_pageSearch + "”"; m_loaded = m_total = 0; fetchPage();
}
void AppController::loadMore() { if (!busy() && hasMore()) fetchPage(true); }
void AppController::fetchPage(bool append) {
    const int generation = m_generation; m_pending = 1;
    if (!append) { m_shelves = {QVariantMap{{"title", m_title}, {"items", QVariantList{}}}}; emit shelvesChanged(); }
    auto q = query({{"Fields", fields}, {"Limit", "60"}, {"StartIndex", QString::number(m_loaded)}, {"SortBy", "SortName"}, {"SortOrder", "Ascending"}});
    if (!m_pageParent.isEmpty()) { q.addQueryItem("ParentId", m_pageParent); q.addQueryItem("Recursive", "false"); }
    else { q.addQueryItem("Recursive", "true"); q.addQueryItem("IncludeItemTypes", m_pageCategory == "Movies" ? "Movie" : m_pageCategory == "Shows" ? "Series" : "Movie,Series,Episode"); }
    if (!m_pageSearch.isEmpty()) q.addQueryItem("SearchTerm", m_pageSearch);
    emit changed();
    m_api.get("Users/" + m_userId + "/Items", q, [this, generation, append](const QJsonObject &r, const QString &e) {
        if (generation != m_generation) return;
        m_pending = 0;
        if (!e.isEmpty()) { setError(e); return; }
        const auto page = items(r); m_loaded += page.size(); m_total = r.value("TotalRecordCount").toInt(m_loaded);
        if (page.isEmpty()) m_total = m_loaded;
        auto shelf = m_shelves.first().toMap(); auto list = append ? shelf.value("items").toList() : QVariantList{};
        list.append(page); shelf["items"] = list; m_shelves[0] = shelf; emit shelvesChanged(); emit changed(); emit browsingReady();
    });
}
void AppController::play(const QVariantMap &item, bool resume) {
    if (m_negotiating || item.value("folder").toBool()) return;
    beginPlay(item, resume ? qint64(item.value("positionTicks").toDouble()) : 0);
}
void AppController::beginPlay(const QVariantMap &item, qint64 startTicks) {
    finishSession(); m_player->stop(); m_currentItem = item;
    const int generation = ++m_playGeneration;
    m_negotiating = true; m_message.clear(); emit changed();
    m_api.post("Items/" + item.value("id").toString() + "/PlaybackInfo",
        Playback::request(m_userId, m_settings->playbackMode(), m_settings->bitrate(), startTicks),
        [this, generation, startTicks](const QJsonObject &r, const QString &e) {
        if (generation != m_playGeneration) return;
        m_negotiating = false;
        if (!e.isEmpty()) { setError(e); return; }
        m_plan = Playback::negotiate(r, m_api.server(), m_currentItem.value("id").toString(), m_api.deviceId(), m_api.token(), m_settings->playbackMode(), startTicks);
        if (!m_plan.valid()) { setError(m_plan.error); return; }
        m_player->load(m_plan.url, m_plan.method == "DirectPlay" ? double(startTicks) / 10000000 : 0,
            m_plan.offset, m_currentItem.value("runtime").toDouble(), m_settings->hardwareDecoding());
        emit changed();
    });
}
void AppController::report(const QString &event) {
    if (m_plan.sessionId.isEmpty()) return;
    m_api.post("Sessions/" + event, {{"ItemId", m_currentItem.value("id").toString()},
        {"MediaSourceId", m_plan.mediaSourceId}, {"PlaySessionId", m_plan.sessionId}, {"PlayMethod", m_plan.method},
        {"PositionTicks", qint64(m_player->position() * 10000000)}, {"IsPaused", m_player->paused()},
        {"CanSeek", true}, {"VolumeLevel", 100}, {"IsMuted", false}});
}
void AppController::finishSession() {
    m_progress.stop();
    if (m_reportedStart) report("Playing/Stopped");
    if (m_plan.method == "Transcode" && !m_plan.sessionId.isEmpty())
        m_api.remove("Videos/ActiveEncodings", query({{"DeviceId", m_api.deviceId()}, {"PlaySessionId", m_plan.sessionId}}));
    m_plan = {}; m_reportedStart = false;
}
void AppController::stopPlayback() {
    ++m_playGeneration; m_negotiating = false; finishSession(); m_player->stop(); emit changed();
}
void AppController::seekRelative(double seconds) {
    if (!m_player->active() || m_negotiating) return;
    const double target = qBound(0.0, m_player->position() + seconds, qMax(0.0, m_player->duration() - 1));
    if (m_plan.method == "Transcode") beginPlay(m_currentItem, qint64(target * 10000000));
    else m_player->seek(target);
}
