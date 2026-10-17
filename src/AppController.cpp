#include "AppController.h"
#include <QJsonArray>
#include <QUrlQuery>
#include <memory>

namespace {
const QString fields = "Overview,Genres,PrimaryImageAspectRatio,ParentId,ChildCount";
QVariantMap sourceDetails(const QJsonObject &source) {
    return {{"id", source.value("Id").toString()}, {"name", source.value("Name").toString("Original")},
        {"audioTracks", Playback::tracks(source, "Audio")}, {"subtitleTracks", Playback::tracks(source, "Subtitle")},
        {"defaultAudio", source.value("DefaultAudioStreamIndex").toInt(-2)},
        {"defaultSubtitle", source.value("DefaultSubtitleStreamIndex").toInt(-1)},
        {"mediaInfo", Playback::mediaInfo(source)}};
}
QUrlQuery query(std::initializer_list<std::pair<QString, QString>> values) {
    QUrlQuery q;
    for (const auto &[key, value] : values) q.addQueryItem(key, value);
    return q;
}
}
AppController::AppController(Settings *settings, MpvPlayer *player, QObject *parent, QNetworkAccessManager *transport)
    : QObject(parent), m_settings(settings), m_player(player), m_themePlayer(new MpvPlayer(this, true)), m_api(settings->value("deviceId").toString(), this, transport) {
    m_themePlayer->setObjectName("themePlayer");
    m_themePlayer->setVolume(settings->themeSongVolume());
    m_themeEnabled = settings->themeSongs();
    m_authenticating = !settings->value("accessToken").toString().isEmpty() && !settings->value("userId").toString().isEmpty();
    connect(settings, &Settings::changed, this, [this] {
        m_themePlayer->setVolume(m_settings->themeSongVolume());
        if (m_themeEnabled == m_settings->themeSongs()) return;
        m_themeEnabled = m_settings->themeSongs();
        stopThemeSong();
        if (m_themeEnabled && !m_detailLoading) fetchThemeSong();
    });
    connect(&m_api, &JellyfinClient::unauthorized, this, [this] { logout(); notify("Your session expired. Sign in again."); });
    connect(player, &MpvPlayer::error, this, [this](const QString &error) { finishSession(); setError(error); });
    connect(player, &MpvPlayer::ended, this, [this] { finishSession(); if (signedIn()) home(); });
    connect(player, &MpvPlayer::loaded, this, [this] {
        if (m_plan.sessionId.isEmpty()) return;
        m_player->setPaused(m_resumePaused);
        int audio = m_playbackOptions.value("audio", -2).toInt();
        int subtitle = m_playbackOptions.value("subtitle", -2).toInt();
        if (audio == -2) audio = m_plan.source.value("DefaultAudioStreamIndex").toInt(-2);
        if (subtitle == -2) subtitle = m_plan.source.value("DefaultSubtitleStreamIndex").toInt(-2);
        if (m_plan.method == "DirectPlay") {
            m_player->selectStream(audio, false);
            m_player->selectStream(subtitle, true);
        } else if (subtitle == -1) m_player->selectStream(-1, true);
        m_reportedStart = true; report("Playing"); m_progress.start();
        for (const auto &value : m_plan.source.value("MediaStreams").toArray()) {
            const auto stream = value.toObject();
            if (stream.value("Type").toString() != "Subtitle") continue;
            if (m_plan.method == "DirectPlay" && !stream.value("IsExternal").toBool()) continue;
            const QString delivery = stream.value("DeliveryUrl").toString();
            if (delivery.isEmpty()) continue;
            auto url = Playback::resourceUrl(m_api.server(), QUrl(delivery));
            if (!Playback::sameOrigin(url, m_api.server())) continue;
            QUrlQuery q(url); q.removeAllQueryItems("api_key"); q.addQueryItem("api_key", m_api.token()); url.setQuery(q);
            const int index = stream.value("Index").toInt();
            const bool selected = subtitle == index || (subtitle == -2 && index == m_plan.source.value("DefaultSubtitleStreamIndex").toInt(-1));
            if (m_plan.method == "DirectPlay" || (selected && stream.value("DeliveryMethod").toString() != "Encode"))
                m_player->addSubtitle(url, selected);
        }
    });
    m_progress.setInterval(10000);
    connect(&m_progress, &QTimer::timeout, this, [this] { report("Playing/Progress"); });
}
void AppController::start() {
    const auto token = m_settings->value("accessToken").toString();
    const auto userId = m_settings->value("userId").toString();
    if (token.isEmpty() || userId.isEmpty()) { m_authenticating = false; emit changed(); return; }
    m_authenticating = true;
    m_api.configure(QUrl(savedServer()), token); m_pending = 1; emit changed();
    m_api.get("Users/Me", {}, [this, userId](const QJsonObject &user, const QString &error) {
        m_pending = 0;
        m_authenticating = false;
        if (!error.isEmpty()) { setError(error); return; }
        m_userId = user.value("Id").toString(userId); m_username = user.value("Name").toString(savedUsername());
        m_profileImage = m_api.userImageUrl(m_userId, user.value("PrimaryImageTag").toString()); home();
    });
}
void AppController::login(const QString &server, const QString &username, const QString &password, bool remember) {
    if (busy()) return;
    QUrl url(server.trimmed());
    if ((url.scheme() != "http" && url.scheme() != "https") || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) {
        setError("Enter a full server address, such as http://192.168.1.10:8096 or https://media.example.com/jellyfin."); return;
    }
    if (username.trimmed().isEmpty()) { setError("Enter your Jellyfin username."); return; }
    m_api.configure(url); m_pending = 1; m_authenticating = true; m_message.clear(); emit changed();
    m_api.post("Users/AuthenticateByName", {{"Username", username.trimmed()}, {"Pw", password}},
        [this, url, remember](const QJsonObject &result, const QString &error) {
        m_pending = 0;
        m_authenticating = false;
        if (!error.isEmpty()) { setError(error); return; }
        const auto user = result.value("User").toObject(); const auto token = result.value("AccessToken").toString();
        if (token.isEmpty() || user.value("Id").toString().isEmpty()) { setError("The server did not return a valid session."); return; }
        m_api.configure(url, token); m_userId = user.value("Id").toString(); m_username = user.value("Name").toString();
        m_profileImage = m_api.userImageUrl(m_userId, user.value("PrimaryImageTag").toString());
        m_settings->put("server", url.toString()); m_settings->put("username", m_username);
        m_settings->clearSession();
        if (remember) { m_settings->put("accessToken", token); m_settings->put("userId", m_userId); }
        home();
    });
}
void AppController::logout() {
    stopPlayback();
    m_api.post("Sessions/Logout", {});
    closeDetails(); m_authenticating = false; m_updatingUserData = false; ++m_generation; m_pending = 0; m_userId.clear(); m_username.clear(); m_profileImage.clear(); m_shelves.clear(); m_libraries.clear(); m_history.clear();
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
    const QString id = o.value("Id").toString();
    const auto tags = o.value("ImageTags").toObject();
    const auto hashes = o.value("ImageBlurHashes").toObject();
    const auto hash = [&hashes](const QString &kind, const QString &tag) {
        return hashes.value(kind).toObject().value(tag).toString();
    };
    QString posterId = id, posterTag = tags.value("Primary").toString(), posterType = "Primary";
    double aspect = o.value("PrimaryImageAspectRatio").toDouble(type == "Episode" ? 16.0 / 9 : 2.0 / 3);
    if (posterTag.isEmpty() && !tags.value("Thumb").toString().isEmpty()) {
        posterTag = tags.value("Thumb").toString(); posterType = "Thumb"; aspect = 16.0 / 9;
    }
    if (posterTag.isEmpty()) {
        posterId = o.value("SeriesId").toString(); posterTag = o.value("SeriesPrimaryImageTag").toString();
        posterType = "Primary"; aspect = 2.0 / 3;
    }
    if (posterTag.isEmpty()) {
        posterId = o.value("ParentPrimaryImageItemId").toString(); posterTag = o.value("ParentPrimaryImageTag").toString();
    }
    QString backdropId = id, backdropTag, backdropType = "Backdrop";
    const auto backdrops = o.value("BackdropImageTags").toArray();
    if (!backdrops.isEmpty()) backdropTag = backdrops.first().toString();
    else {
        backdropId = o.value("ParentBackdropItemId").toString();
        const auto inherited = o.value("ParentBackdropImageTags").toArray();
        if (!inherited.isEmpty()) backdropTag = inherited.first().toString();
    }
    if (backdropTag.isEmpty()) {
        backdropId = id; backdropTag = tags.value("Thumb").toString(); backdropType = "Thumb";
        if (backdropTag.isEmpty() && type == "Episode" && aspect > 1 && posterId == id) {
            backdropTag = posterTag; backdropType = posterType;
        }
    }
    if (backdropTag.isEmpty()) {
        backdropId = o.value("ParentThumbItemId").toString();
        backdropTag = o.value("ParentThumbImageTag").toString(); backdropType = "Thumb";
    }
    QVariantList people, chapters, sources;
    for (const auto &value : o.value("MediaSources").toArray()) sources.append(sourceDetails(value.toObject()));
    if (sources.isEmpty() && !o.value("MediaStreams").toArray().isEmpty()) sources.append(sourceDetails(o));
    for (const auto &v : o.value("People").toArray()) {
        const auto person = v.toObject();
        people.append(QVariantMap{{"name", person.value("Name").toString()}, {"role", person.value("Role").toString(person.value("Type").toString())}});
    }
    for (const auto &v : o.value("Chapters").toArray()) {
        const auto chapter = v.toObject();
        const auto start = chapter.value("StartPositionTicks");
        if (!start.isDouble() || start.toDouble() < 0) continue;
        chapters.append(QVariantMap{{"name", chapter.value("Name").toString()}, {"ticks", chapter.value("StartPositionTicks").toDouble()}});
    }
    return {{"id", id}, {"title", o.value("Name").toString()}, {"subtitle", subtitle}, {"type", type},
        {"overview", o.value("Overview").toString()}, {"runtime", ticks / 10000000}, {"positionTicks", position},
        {"progress", ticks > 0 ? qBound(0.0, position / ticks, 1.0) : 0},
        {"poster", m_api.imageUrl(posterId, posterType, posterTag, aspect > 1 ? 1280 : 640)},
        {"posterBlurHash", hash(posterType, posterTag)}, {"imageAspect", aspect},
        {"backdrop", m_api.imageUrl(backdropId, backdropType == "Backdrop" ? "Backdrop/0" : backdropType, backdropTag, 3840)},
        {"backdropBlurHash", hash(backdropType, backdropTag)},
        {"seriesId", o.value("SeriesId").toString()}, {"seriesName", o.value("SeriesName").toString()},
        {"seasonNumber", o.value("ParentIndexNumber").toVariant()}, {"episodeNumber", o.value("IndexNumber").toVariant()},
        {"collectionType", o.value("CollectionType").toString()}, {"mediaSources", sources}, {"trickplay", o.value("Trickplay").toObject().toVariantMap()},
        {"seasonId", o.value("SeasonId").toString()}, {"seasonName", o.value("SeasonName").toString()},
        {"played", data.value("Played").toBool()}, {"favorite", data.value("IsFavorite").toBool()},
        {"rating", o.value("OfficialRating").toString()}, {"communityRating", o.value("CommunityRating").toDouble()},
        {"genres", o.value("Genres").toArray().toVariantList()}, {"people", people}, {"chapters", chapters},
        {"tagline", o.value("Taglines").toArray().isEmpty() ? QString{} : o.value("Taglines").toArray().first().toString()},
        {"childCount", o.value("ChildCount").toInt()}, {"playable", type == "Movie" || type == "Episode" || type == "Video" || type == "MusicVideo"},
        {"folder", o.value("IsFolder").toBool() || type == "Series" || type == "Season" || type == "CollectionFolder" || type == "BoxSet"}};
}

QVariantList AppController::items(const QJsonObject &response) const {
    QVariantList result;
    for (const auto &v : response.value("Items").toArray()) result.append(normalize(v.toObject()));
    return result;
}
void AppController::home(const QString &category) {
    if (!signedIn()) return;
    stopThemeSong();
    ++m_generation; m_history.clear(); m_pageParent.clear(); m_pageSearch.clear(); m_pageCategory = category; m_pageType.clear(); m_pageSeries.clear(); m_filter = "All"; m_sort = "Name";
    m_title = category; m_loaded = m_total = 0; m_shelves.clear(); m_message.clear();
    if (category != "Home") { fetchPage(); return; }
    m_pending = 3; const int generation = m_generation;
    for (const auto &title : {"Continue watching", "Up next"})
        m_shelves.append(QVariantMap{{"title", title}, {"items", QVariantList{}}, {"loading", true}});
    emit shelvesChanged();
    const auto done = [this, generation](int index, const QJsonObject &result, const QString &error) {
        if (generation != m_generation) return;
        auto shelf = m_shelves[index].toMap(); shelf["items"] = items(result); shelf["loading"] = false; m_shelves[index] = shelf;
        --m_pending; if (!error.isEmpty()) m_message = error;
        emit shelvesChanged(); emit changed(); if (m_pending == 0) emit browsingReady();
    };
    emit changed();
    m_api.get("Users/" + m_userId + "/Views", {}, [this, generation, done](const QJsonObject &r, const QString &e) {
        if (generation != m_generation) return;
        m_libraries.clear();
        for (const auto &entry : items(r)) {
            const auto library = entry.toMap();
            const auto type = library.value("collectionType").toString();
            if (!QStringList{"", "movies", "tvshows", "homevideos", "musicvideos", "boxsets"}.contains(type)) continue;
            m_libraries.append(library);
            const int index = m_shelves.size();
            m_shelves.append(QVariantMap{{"title", "Recently added · " + library.value("title").toString()}, {"items", QVariantList{}}, {"loading", true}});
            ++m_pending;
            m_api.get("Users/" + m_userId + "/Items", query({{"ParentId", library.value("id").toString()},
                {"Recursive", "true"}, {"IncludeItemTypes", type == "tvshows" ? "Series" : type == "movies" ? "Movie" : "Movie,Series,Video,MusicVideo"},
                {"SortBy", "DateCreated"}, {"SortOrder", "Descending"}, {"Limit", "24"}, {"Fields", fields}}),
                [done, index](const auto &result, const auto &error) { done(index, result, error); });
        }
        --m_pending; if (!e.isEmpty()) m_message = e;
        emit librariesChanged(); emit shelvesChanged(); emit changed();
        if (m_pending == 0) emit browsingReady();
    });
    m_api.get("Users/" + m_userId + "/Items/Resume", query({{"Limit", "20"}, {"IsMissing", "false"}, {"Fields", fields}, {"MediaTypes", "Video"}}), [done](const auto &r, const auto &e) { done(0, r, e); });
    m_api.get("Shows/NextUp", query({{"UserId", m_userId}, {"Limit", "20"}, {"IsMissing", "false"}, {"Fields", fields}}), [done](const auto &r, const auto &e) { done(1, r, e); });
}
void AppController::open(const QVariantMap &item) {
    if (!item.value("folder").toBool()) return;
    if (item.value("type") == "Series") { showDetails(item); return; }
    if (item.value("type") == "Season") { openSeries(item); return; }
    stopThemeSong();
    m_history.append({{"title", m_title}, {"parent", m_pageParent}, {"search", m_pageSearch}, {"category", m_pageCategory}, {"type", m_pageType}, {"series", m_pageSeries}, {"filter", m_filter}, {"sort", m_sort}});
    ++m_generation; m_title = item.value("title").toString(); m_pageParent = item.value("id").toString();
    m_pageSearch.clear(); m_pageCategory.clear(); m_pageType = item.value("type").toString(); m_pageSeries = item.value("seriesId").toString();
    m_filter = "All"; m_sort = "Name"; m_loaded = m_total = 0; fetchPage();
}
void AppController::openLibrary(const QVariantMap &item) {
    if (!signedIn() || !item.value("folder").toBool()) return;
    m_history.clear(); m_title = "Home"; m_pageCategory = "Home";
    m_pageParent.clear(); m_pageSearch.clear(); m_pageType.clear(); m_pageSeries.clear();
    open(item);
}
void AppController::back() {
    if (m_history.isEmpty()) { home(); return; }
    auto previous = m_history.takeLast();
    if (previous.value("category") == "Home") { home(); return; }
    ++m_generation; m_title = previous.value("title").toString(); m_pageParent = previous.value("parent").toString();
    m_pageSearch = previous.value("search").toString(); m_pageCategory = previous.value("category").toString();
    m_pageType = previous.value("type").toString(); m_pageSeries = previous.value("series").toString();
    m_filter = previous.value("filter").toString(); m_sort = previous.value("sort").toString();
    m_loaded = m_total = 0; fetchPage();
}
void AppController::search(const QString &text) {
    if (text.trimmed().isEmpty() || !signedIn()) return;
    stopThemeSong();
    ++m_generation; m_history.clear(); m_pageParent.clear(); m_pageCategory.clear(); m_pageSearch = text.trimmed();
    m_pageType.clear(); m_pageSeries.clear(); m_filter = "All"; m_sort = "Name";
    m_title = "Results for “" + m_pageSearch + "”"; m_loaded = m_total = 0; fetchPage();
}
void AppController::loadMore() { if (!busy() && hasMore()) fetchPage(true); }
void AppController::showDetails(const QVariantMap &item) {
    if (!signedIn() || item.value("id").toString().isEmpty()) return;
    // Invalidate old requests without interrupting the inherited series theme.
    ++m_themeGeneration;
    auto context = item.value("seriesId").toString();
    if (context.isEmpty()) context = item.value("id").toString();
    if (!m_themeContext.isEmpty() && context != m_themeContext) stopThemeSong();
    const int generation = ++m_detailGeneration;
    m_detail = item; m_detailLoading = true; emit detailsChanged();
    m_api.get("Users/" + m_userId + "/Items/" + item.value("id").toString(),
        query({{"Fields", fields + ",People,Studios,Taglines,Chapters,MediaStreams,MediaSources"}}),
        [this, generation](const QJsonObject &result, const QString &error) {
        if (generation != m_detailGeneration) return;
        m_detailLoading = false;
        if (!error.isEmpty()) { stopThemeSong(); emit detailsChanged(); setError(error); return; }
        if (result.value("Id").toString() != m_detail.value("id").toString()) {
            emit detailsChanged(); setError("The server did not return the requested title."); return;
        }
        m_detail = normalize(result);
        if (m_detail.value("type") == "Series") m_detail["seasonsLoading"] = true;
        emit detailsChanged();
        fetchThemeSong();
        if (m_detail.value("type") == "Series") fetchDetailSeasons(generation);
        if (m_detail.value("type") == "Episode" && !m_detail.value("seriesId").toString().isEmpty()) {
            m_api.get("Shows/" + m_detail.value("seriesId").toString() + "/Episodes",
                query({{"UserId", m_userId}, {"StartItemId", m_detail.value("id").toString()}, {"Limit", "2"}, {"Fields", fields}, {"IsMissing", "false"}}),
                [this, generation](const QJsonObject &r, const QString &e) {
                if (generation != m_detailGeneration || !e.isEmpty()) return;
                const auto episodes = items(r);
                if (episodes.size() > 1) m_detail["nextEpisode"] = episodes[1];
                emit detailsChanged();
            });
        }
    });
}
void AppController::fetchDetailSeasons(int generation) {
    const auto seriesId = m_detail.value("id").toString();
    m_api.get("Shows/" + seriesId + "/Seasons", query({{"UserId", m_userId}, {"Fields", fields}, {"IsMissing", "false"}}),
        [this, generation, seriesId](const QJsonObject &result, const QString &error) {
        if (generation != m_detailGeneration) return;
        auto seasons = std::make_shared<QVariantList>();
        for (const auto &value : items(result)) {
            const auto season = value.toMap();
            if (!season.value("id").toString().isEmpty())
                seasons->append(QVariantMap{{"id", season.value("id")}, {"title", season.value("title")}, {"items", QVariantList{}}, {"loading", true}});
        }
        if (!error.isEmpty() || seasons->isEmpty()) {
            m_detail["seasonsLoading"] = false;
            m_detail["seasonsError"] = error;
            m_detail["seasons"] = QVariantList{};
            emit detailsChanged(); return;
        }
        auto pending = std::make_shared<int>(seasons->size());
        m_detail["seasons"] = *seasons;
        emit detailsChanged();
        for (int index = 0; index < seasons->size(); ++index) {
            const auto seasonId = seasons->at(index).toMap().value("id").toString();
            m_api.get("Shows/" + seriesId + "/Episodes",
                query({{"UserId", m_userId}, {"SeasonId", seasonId}, {"Fields", fields}, {"IsMissing", "false"}}),
                [this, generation, seasons, pending, index](const QJsonObject &r, const QString &e) {
                if (generation != m_detailGeneration) return;
                auto section = seasons->at(index).toMap();
                section["items"] = e.isEmpty() ? items(r) : QVariantList{};
                section["error"] = e;
                section["loading"] = false;
                (*seasons)[index] = section;
                m_detail["seasons"] = *seasons;
                m_detail["seasonsLoading"] = --*pending > 0;
                emit detailsChanged();
            });
        }
    });
}
void AppController::closeDetails() {
    stopThemeSong();
    ++m_detailGeneration; m_detailLoading = false; m_detail.clear(); emit detailsChanged();
}
void AppController::stopThemeSong() {
    ++m_themeGeneration;
    m_themeContext.clear(); m_themeSongId.clear();
    m_themePlayer->stop();
}
void AppController::fetchThemeSong() {
    const auto id = m_detail.value("id").toString();
    if (!signedIn() || id.isEmpty() || !m_settings->themeSongs() || m_player->active() || m_negotiating) return;
    const int generation = ++m_themeGeneration;
    m_api.get("Items/" + QString::fromLatin1(QUrl::toPercentEncoding(id)) + "/ThemeSongs",
        query({{"UserId", m_userId}, {"InheritFromParent", "true"}}),
        [this, generation](const QJsonObject &result, const QString &error) {
        if (generation != m_themeGeneration || !m_settings->themeSongs() || m_player->active() || m_negotiating) return;
        // Theme media is optional; missing or unplayable themes must not interrupt browsing.
        const auto songs = result.value("Items").toArray();
        if (!error.isEmpty() || songs.isEmpty()) { stopThemeSong(); return; }
        const auto songId = songs.first().toObject().value("Id").toString();
        if (songId.isEmpty()) { stopThemeSong(); return; }
        m_themeContext = m_detail.value("seriesId").toString();
        if (m_themeContext.isEmpty()) m_themeContext = m_detail.value("id").toString();
        if (songId == m_themeSongId) return;
        m_themeSongId = songId;
        auto url = Playback::endpoint(m_api.server(), "Audio/" + QString::fromLatin1(QUrl::toPercentEncoding(songId)) + "/stream");
        url.setQuery(query({{"static", "true"}, {"api_key", m_api.token()}}));
        m_themePlayer->load(url, 0, 0, 0, false);
    });
}
void AppController::openSeries(const QVariantMap &item) {
    const auto id = item.value("seriesId").toString();
    if (id.isEmpty()) return;
    showDetails({{"id", id}, {"title", item.value("seriesName")}, {"type", "Series"}, {"folder", true}});
}
void AppController::openSeason(const QVariantMap &item) {
    openSeries(item);
}
void AppController::setLibraryFilter(const QString &filter) {
    if (!QStringList{"All", "Favorites", "Unwatched", "Watched"}.contains(filter) || m_pageCategory == "Home" || m_pageType == "Series" || m_pageType == "Season") return;
    ++m_generation; m_filter = filter; m_loaded = m_total = 0; fetchPage();
}
void AppController::setSortOrder(const QString &sort) {
    if (!QStringList{"Name", "Recently added", "Rating"}.contains(sort) || m_pageCategory == "Home" || m_pageType == "Series" || m_pageType == "Season") return;
    ++m_generation; m_sort = sort; m_loaded = m_total = 0; fetchPage();
}
void AppController::setFavorite(const QVariantMap &item, bool favorite) { changeUserData(item, favorite, true); }
void AppController::setPlayed(const QVariantMap &item, bool played) { changeUserData(item, played, false); }
void AppController::changeUserData(const QVariantMap &item, bool value, bool favorite) {
    const QString id = item.value("id").toString();
    if (!signedIn() || id.isEmpty() || m_updatingUserData) return;
    m_updatingUserData = true; emit detailsChanged();
    const QString path = "Users/" + m_userId + (favorite ? "/FavoriteItems/" : "/PlayedItems/") + id;
    const auto done = [this, id, favorite, value](const QJsonObject &r, const QString &error) {
        m_updatingUserData = false;
        if (!error.isEmpty()) { emit detailsChanged(); setError(error); return; }
        auto data = r;
        // Some compatible servers return 204. Apply the confirmed action in that case.
        if (!data.contains(favorite ? "IsFavorite" : "Played")) data[favorite ? "IsFavorite" : "Played"] = value;
        if (!favorite && !data.contains("PlaybackPositionTicks")) data["PlaybackPositionTicks"] = 0;
        updateUserData(id, data);
    };
    if (value) m_api.post(path, {}, done);
    else m_api.remove(path, {}, done);
}
void AppController::updateUserData(const QString &id, const QJsonObject &data) {
    const auto update = [&id, &data](QVariantMap item) {
        if (item.value("id").toString() != id) return item;
        if (data.contains("IsFavorite")) item["favorite"] = data.value("IsFavorite").toBool();
        if (data.contains("Played")) item["played"] = data.value("Played").toBool();
        if (data.contains("PlaybackPositionTicks")) {
            const double position = data.value("PlaybackPositionTicks").toDouble();
            item["positionTicks"] = position;
            const double ticks = item.value("runtime").toDouble() * 10000000;
            item["progress"] = ticks > 0 ? qBound(0.0, position / ticks, 1.0) : 0;
        }
        return item;
    };
    m_detail = update(m_detail); m_currentItem = update(m_currentItem);
    for (auto &v : m_shelves) {
        auto shelf = v.toMap(); auto list = shelf.value("items").toList();
        for (auto &entry : list) entry = update(entry.toMap());
        shelf["items"] = list; v = shelf;
    }
    emit detailsChanged(); emit shelvesChanged(); emit changed();
}
void AppController::fetchPage(bool append) {
    const int generation = m_generation; m_pending = 1;
    if (!append) { m_shelves = {QVariantMap{{"title", m_title}, {"items", QVariantList{}}, {"loading", true}}}; emit shelvesChanged(); }
    auto q = query({{"Fields", fields}, {"Limit", "60"}, {"StartIndex", QString::number(m_loaded)}, {"IsMissing", "false"}, {"SortBy", "SortName"}, {"SortOrder", "Ascending"}});
    if (!m_pageParent.isEmpty()) { q.addQueryItem("ParentId", m_pageParent); q.addQueryItem("Recursive", "false"); }
    else { q.addQueryItem("Recursive", "true"); q.addQueryItem("IncludeItemTypes", m_pageCategory == "Movies" ? "Movie" : m_pageCategory == "Shows" ? "Series" : "Movie,Series,Episode"); }
    if (!m_pageSearch.isEmpty()) q.addQueryItem("SearchTerm", m_pageSearch);
    if (m_filter == "Favorites") q.addQueryItem("Filters", "IsFavorite");
    else if (m_filter == "Unwatched") q.addQueryItem("IsPlayed", "false");
    else if (m_filter == "Watched") q.addQueryItem("IsPlayed", "true");
    if (m_sort != "Name") {
        q.removeAllQueryItems("SortBy"); q.removeAllQueryItems("SortOrder");
        q.addQueryItem("SortBy", m_sort == "Recently added" ? "DateCreated" : "CommunityRating");
        q.addQueryItem("SortOrder", "Descending");
    }
    const QString path = "Users/" + m_userId + "/Items";
    emit changed();
    m_api.get(path, q, [this, generation, append](const QJsonObject &r, const QString &e) {
        if (generation != m_generation) return;
        m_pending = 0;
        auto shelf = m_shelves.first().toMap(); shelf["loading"] = false; m_shelves[0] = shelf;
        emit shelvesChanged();
        if (!e.isEmpty()) { setError(e); return; }
        const auto page = items(r); m_loaded += page.size(); m_total = r.value("TotalRecordCount").toInt(m_loaded);
        if (page.isEmpty()) m_total = m_loaded;
        auto list = append ? shelf.value("items").toList() : QVariantList{};
        list.append(page); shelf["items"] = list; m_shelves[0] = shelf; emit shelvesChanged(); emit changed(); emit browsingReady();
    });
}
QVariantMap AppController::currentSource() const { return sourceDetails(m_plan.source); }
void AppController::play(const QVariantMap &item, bool resume, const QVariantMap &options) {
    if (!signedIn() || m_negotiating || item.value("folder").toBool() || item.value("id").toString().isEmpty()) return;
    m_playbackOptions = options;
    m_segments.clear(); m_serverSegments.clear();
    beginPlay(item, resume ? qint64(item.value("positionTicks").toDouble()) : 0);
    fetchPlaybackMarkers();
}
void AppController::fetchPlaybackMarkers() {
    const int generation = ++m_markerGeneration;
    const auto itemId = m_currentItem.value("id").toString();
    refreshSegments();
    m_api.get("MediaSegments/" + itemId, {},
        [this, generation](const QJsonObject &response, const QString &error) {
        if (generation != m_markerGeneration) return;
        // Segment providers are optional, including on servers predating this API.
        m_serverSegments = error.isEmpty() ? Playback::segments(response) : QVariantList{};
        refreshSegments();
    });
    // Shelf items omit chapters; fetch them without delaying playback.
    m_api.get("Items/" + itemId, query({{"UserId", m_userId}}),
        [this, generation, itemId](const QJsonObject &response, const QString &error) {
        if (generation != m_markerGeneration || !error.isEmpty() ||
            response.value("Id").toString() != itemId || !response.value("Chapters").isArray()) return;
        m_currentItem["chapters"] = normalize(response).value("chapters");
        refreshSegments();
    });
}
void AppController::refreshSegments() {
    m_segments = m_serverSegments.isEmpty()
        ? Playback::chapterSegments(m_currentItem.value("chapters").toList(), m_currentItem.value("runtime").toDouble())
        : m_serverSegments;
    emit changed();
}
void AppController::beginPlay(const QVariantMap &item, qint64 startTicks, bool paused) {
    stopThemeSong();
    m_negotiating = true; emit changed();
    finishSession(); m_player->stop(); m_currentItem = item; m_resumePaused = paused;
    const int generation = ++m_playGeneration;
    m_message.clear(); emit changed();
    const int quality = m_playbackOptions.value("quality", 0).toInt();
    const QString mode = quality > 0 ? "transcode" : quality == -1 ? "direct" : m_settings->playbackMode();
    auto request = Playback::request(m_userId, mode, quality > 0 ? quality : m_settings->bitrate(), startTicks);
    const auto sourceId = m_playbackOptions.value("sourceId").toString();
    if (!sourceId.isEmpty()) request["MediaSourceId"] = sourceId;
    if (m_playbackOptions.value("audio", -2).toInt() >= 0) request["AudioStreamIndex"] = m_playbackOptions.value("audio").toInt();
    if (m_playbackOptions.value("subtitle", -2).toInt() >= -1) request["SubtitleStreamIndex"] = m_playbackOptions.value("subtitle").toInt();
    m_api.post("Items/" + item.value("id").toString() + "/PlaybackInfo", request,
        [this, generation, startTicks, mode, sourceId](const QJsonObject &r, const QString &e) {
        if (generation != m_playGeneration) return;
        m_negotiating = false;
        if (!e.isEmpty()) { setError(e); return; }
        m_plan = Playback::negotiate(r, m_api.server(), m_currentItem.value("id").toString(), m_api.deviceId(), m_api.token(), mode, startTicks, sourceId);
        if (!m_plan.valid()) { setError(m_plan.error); return; }
        m_playbackOptions["sourceId"] = m_plan.mediaSourceId;
        m_player->load(m_plan.url, m_plan.method == "DirectPlay" ? double(startTicks) / 10000000 : 0,
            m_plan.offset, m_currentItem.value("runtime").toDouble(), m_settings->hardwareDecoding());
        emit changed();
    });
}
void AppController::setQuality(int bitrate) {
    if (!m_player->active() || m_negotiating || !QList<int>{-1, 0, 4000000, 8000000, 20000000, 40000000, 80000000}.contains(bitrate)) return;
    m_playbackOptions["quality"] = bitrate;
    beginPlay(m_currentItem, qint64(m_player->position() * 10000000), m_player->paused());
}
void AppController::setPlaybackTrack(bool subtitle, int index) {
    if (!m_player->active() || m_negotiating) return;
    bool valid = index == -2 || (subtitle && index == -1);
    for (const auto &track : Playback::tracks(m_plan.source, subtitle ? "Subtitle" : "Audio"))
        valid |= track.toMap().value("index").toInt() == index;
    if (!valid) return;
    m_playbackOptions[subtitle ? "subtitle" : "audio"] = index;
    beginPlay(m_currentItem, qint64(m_player->position() * 10000000), m_player->paused());
}
void AppController::skipSegment() {
    if (!m_player->active() || m_negotiating) return;
    const auto segment = Playback::activeSegment(m_segments, m_player->position());
    if (segment.isEmpty()) return;
    if (m_player->duration() > 0 && segment.value("end").toDouble() >= m_player->duration()) stopPlayback();
    else seekRelative(segment.value("end").toDouble() - m_player->position());
}
void AppController::report(const QString &event) {
    if (m_plan.sessionId.isEmpty()) return;
    m_api.post("Sessions/" + event, {{"ItemId", m_currentItem.value("id").toString()},
        {"MediaSourceId", m_plan.mediaSourceId}, {"PlaySessionId", m_plan.sessionId}, {"PlayMethod", m_plan.method},
        {"PositionTicks", qint64(m_player->position() * 10000000)}, {"IsPaused", m_player->paused()},
        {"AudioStreamIndex", m_playbackOptions.value("audio", -2).toInt() == -2 ? m_plan.source.value("DefaultAudioStreamIndex").toInt(-1) : m_playbackOptions.value("audio").toInt()},
        {"SubtitleStreamIndex", m_playbackOptions.value("subtitle", -2).toInt() == -2 ? m_plan.source.value("DefaultSubtitleStreamIndex").toInt(-1) : m_playbackOptions.value("subtitle").toInt()},
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
    ++m_playGeneration; ++m_markerGeneration; m_segments.clear(); m_serverSegments.clear(); m_negotiating = false; finishSession(); m_player->stop(); emit changed();
}
void AppController::seekRelative(double seconds) {
    if (!m_player->active() || m_negotiating) return;
    const double target = qBound(0.0, m_player->position() + seconds, qMax(0.0, m_player->duration() - 1));
    if (m_plan.method == "Transcode") beginPlay(m_currentItem, qint64(target * 10000000), m_player->paused());
    else m_player->seek(target);
}
QVariantMap AppController::trickplayFrame(double seconds) const {
    const auto versions = m_currentItem.value("trickplay").toMap();
    auto sizes = versions.value(m_plan.mediaSourceId).toMap();
    QVariantMap info;
    // Use the smallest available thumbnail to keep scrubbing responsive.
    for (const auto &value : sizes) {
        const auto candidate = value.toMap();
        if (candidate.value("Width").toInt() > 0 && (info.isEmpty() || candidate.value("Width").toInt() < info.value("Width").toInt())) info = candidate;
    }
    const int columns = info.value("TileWidth").toInt(), rows = info.value("TileHeight").toInt();
    const int interval = info.value("Interval").toInt(), count = info.value("ThumbnailCount").toInt();
    const int width = info.value("Width").toInt(), height = info.value("Height").toInt();
    if (columns <= 0 || rows <= 0 || interval <= 0 || count <= 0 || width <= 0 || height <= 0) return {};
    const qint64 frame = qBound(qint64(0), qint64(qMax(0.0, seconds) * 1000 / interval), qint64(count - 1));
    const qint64 perSheet = qint64(columns) * rows;
    auto url = Playback::endpoint(m_api.server(), "Videos/" + m_currentItem.value("id").toString() + "/Trickplay/" + QString::number(width) + "/" + QString::number(frame / perSheet) + ".jpg");
    url.setQuery(query({{"ApiKey", m_api.token()}, {"MediaSourceId", m_plan.mediaSourceId}}));
    return {{"url", url.toString(QUrl::FullyEncoded)}, {"width", width}, {"height", height},
        {"columns", columns}, {"rows", rows}, {"column", frame % perSheet % columns}, {"row", frame % perSheet / columns}};
}
