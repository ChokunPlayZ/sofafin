#include "AppController.h"
#include "ControllerInput.h"
#include "MpvVideo.h"
#include "BlurHash.h"
#include <QDesktopServices>
#include <QQmlComponent>
#include "FakeNetwork.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QProcess>
#include <QStandardPaths>
#include <QJsonArray>
#include <QBuffer>
#include <QDataStream>
#include <QTcpServer>
#include <QTcpSocket>
#include <QRegularExpression>
#include <QScopeGuard>
#include <mpv/render.h>

class NativeTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<FakeNetwork> m_network;
    std::unique_ptr<MpvPlayer> m_player;
    std::unique_ptr<ControllerInput> m_input;
    std::unique_ptr<AppController> m_app;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QQuickWindow *m_window = nullptr;
    QUrl m_keyboardUrl;
    QJsonObject movie(const QString &id = "movie", const QString &title = "The Long Way Home") {
        return {{"Id", id}, {"Name", title}, {"Type", "Movie"}, {"ProductionYear", 2025},
            {"RunTimeTicks", 72000000000LL}, {"Overview", "A small crew crosses an unfamiliar ocean in search of a place to call home."},
            {"UserData", QJsonObject{{"PlaybackPositionTicks", 1200000000}}}};
    }
    QJsonObject episode(const QString &id) {
        const int number = id.section('-', -1).toInt();
        return {{"Id", id}, {"Name", QString("Episode %1").arg(number)}, {"Type", "Episode"},
            {"SeriesId", "series"}, {"SeriesName", "A series"},
            {"ParentIndexNumber", number == 1 ? 1 : 2}, {"IndexNumber", number == 1 ? 1 : number - 1},
            {"RunTimeTicks", 1200000000}, {"UserData", QJsonObject{{"PlaybackPositionTicks", 200000000}}}};
    }
    QVariantMap playingEpisode(double position = 0) {
        return {{"id", "episode-1"}, {"title", "Episode 1"}, {"type", "Episode"},
            {"seriesId", "series"}, {"seriesName", "A series"}, {"seasonNumber", 1}, {"episodeNumber", 1},
            {"runtime", 120}, {"positionTicks", position * 10000000}};
    }
    void serveEpisodeSequence(int delay = 0, int status = 200) {
        const auto original = m_network->respond;
        m_network->respond = [this, original, delay, status](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.endsWith("Shows/series/Episodes")) {
                const auto id = QUrlQuery(request.request.url()).queryItemValue("StartItemId");
                QJsonArray episodes{episode(id)};
                const int number = id.section('-', -1).toInt();
                if (number < 3) episodes.append(episode(QString("episode-%1").arg(number + 1)));
                return FixtureResponse{QJsonDocument(QJsonObject{{"Items", episodes}}).toJson(), status, delay};
            }
            if (path.endsWith("/episode-2/PlaybackInfo")) return FixtureResponse{R"({"PlaySessionId":"next-session",
                "MediaSources":[{"Id":"next-source","SupportsDirectPlay":true,"TranscodingUrl":"/base/Videos/episode-2/master.m3u8"}]})"};
            if (path.contains("/Items/episode-") && !path.endsWith("PlaybackInfo"))
                return FixtureResponse{QJsonDocument(episode(path.section('/', -1))).toJson()};
            return original(request);
        };
    }
    bool signIn() {
        m_app->login("https://fixture.invalid/base", "fixture", "fixture-password", false);
        return QTest::qWaitFor([this] { return m_app->signedIn() && !m_app->busy(); }, 3000);
    }
    QObject *object(const char *name) {
        if (auto *found = m_window->findChild<QObject *>(QString::fromUtf8(name))) return found;
        std::function<QQuickItem *(QQuickItem *)> visualFind = [&](QQuickItem *parent) -> QQuickItem * {
            if (parent->objectName() == QString::fromUtf8(name)) return parent;
            for (auto *child : parent->childItems()) if (auto *found = visualFind(child)) return found;
            return nullptr;
        };
        return visualFind(m_window->contentItem());
    }
private slots:
    void initTestCase() {
        QVERIFY(m_config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_config.path());
        qmlRegisterType<MpvVideo>("JFGamescope", 1, 0, "MpvVideo");
    }
    void init() {
        m_settings = std::make_unique<Settings>(); m_settings->clearSession(); m_settings->setPlaybackMode("auto");
        m_settings->setThemeSongVolume(70); m_settings->setUiScale(1.0);
        m_network = std::make_unique<FakeNetwork>();
        m_network->respond = [this](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            QJsonObject result;
            if (path.endsWith("AuthenticateByName")) result = {{"AccessToken", "fixture-token"}, {"User", QJsonObject{{"Id", "user"}, {"Name", "Fixture Viewer"}}}};
            else if (path.endsWith("/Views")) result = {{"Items", QJsonArray{QJsonObject{{"Id", "library"}, {"Name", "Movies"}, {"Type", "CollectionFolder"}, {"IsFolder", true}}}}};
            else if (path.endsWith("PlaybackInfo")) {
                const bool direct = QJsonDocument::fromJson(request.body).object()["EnableDirectPlay"].toBool();
                result = {{"PlaySessionId", "session"}, {"MediaSources", QJsonArray{QJsonObject{
                    {"Id", "source"}, {"SupportsDirectPlay", direct}, {"TranscodingUrl", "/base/Videos/movie/master.m3u8"},
                    {"MediaStreams", QJsonArray{QJsonObject{{"Type", "Audio"}, {"Index", 1}, {"DisplayTitle", "English"}},
                        QJsonObject{{"Type", "Audio"}, {"Index", 2}, {"DisplayTitle", "Thai"}},
                        QJsonObject{{"Type", "Subtitle"}, {"Index", 3}, {"DisplayTitle", "English"}}}}}}}};
            } else if (path.contains("/FavoriteItems/") || path.contains("/PlayedItems/")) {
                const bool added = request.operation == QNetworkAccessManager::PostOperation;
                result = path.contains("/FavoriteItems/") ? QJsonObject{{"IsFavorite", added}} : QJsonObject{{"Played", added}, {"PlaybackPositionTicks", 0}};
            } else if (path.contains("/Items/") && !path.endsWith("/Resume")) {
                const QString id = path.section('/', -1);
                result = movie(id);
            } else if (path.endsWith("Items") || path.endsWith("Resume") || path.endsWith("NextUp")) {
                QJsonArray titles;
                const int start = QUrlQuery(request.request.url()).queryItemValue("StartIndex").toInt();
                const int count = path.endsWith("Items") ? (start == 0 ? 60 : 1) : 3;
                for (int i = 0; i < count; ++i) titles.append(movie(QString("movie-%1").arg(i + start), i == 0 ? "The Long Way Home" : QString("Another good watch %1").arg(i + start)));
                result = {{"Items", titles}, {"TotalRecordCount", 61}};
            }
            return FixtureResponse{QJsonDocument(result).toJson()};
        };
        m_player = std::make_unique<MpvPlayer>(); m_input = std::make_unique<ControllerInput>();
        m_app = std::make_unique<AppController>(m_settings.get(), m_player.get(), nullptr, m_network.get());
        m_engine = std::make_unique<QQmlApplicationEngine>();
        m_engine->addImageProvider("blurhash", new BlurHashProvider);
        m_keyboardUrl = QUrl(); QDesktopServices::setUrlHandler("steam", this, "captureKeyboardUrl");
        auto *ctx = m_engine->rootContext();
        ctx->setContextProperty("App", m_app.get()); ctx->setContextProperty("Player", m_player.get());
        ctx->setContextProperty("Preferences", m_settings.get()); ctx->setContextProperty("Controller", m_input.get());
        ctx->setContextProperty("startWindowed", true); ctx->setContextProperty("startFullscreen", false);
        m_engine->load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first()); QVERIFY(m_window);
        m_window->requestActivate(); QTest::qWait(30);
    }
    void cleanup() {
        QDesktopServices::unsetUrlHandler("steam");
        m_engine.reset(); m_app.reset(); m_input.reset(); m_player.reset(); m_network.reset(); m_settings.reset();
    }
    void captureKeyboardUrl(const QUrl &url) { m_keyboardUrl = url; }
    void steamKeyboardAndSignInFocus() {
        auto *server = qobject_cast<QQuickItem *>(object("serverField")); QVERIFY(server); server->forceActiveFocus();
        const auto preview = qEnvironmentVariable("JF_LOGIN_SCREENSHOT");
        if (!preview.isEmpty()) QVERIFY(m_window->grabWindow().save(preview));
        QTest::keyClick(m_window, Qt::Key_F2);
        QTRY_COMPARE(m_keyboardUrl.scheme(), "steam");
        QCOMPARE(m_keyboardUrl.host(), "open"); QCOMPARE(m_keyboardUrl.path(), "/keyboard");
        const QUrlQuery parameters(m_keyboardUrl);
        QCOMPARE(parameters.queryItemValue("Mode"), "0");
        QCOMPARE(parameters.queryItemValue("Width").toInt(), qRound(server->width()));
        QVERIFY(parameters.queryItemValue("YPosition").toInt() > 0);
        QVERIFY(server->hasActiveFocus());
        for (const char c : QByteArray("https://example.com")) QTest::keyClick(m_window, c);
        QVERIFY(server->property("text").toString().endsWith("https://example.com"));
        // Steam's Done/Enter advances focus and must not reopen the keyboard.
        m_keyboardUrl = QUrl();
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("usernameField")->property("activeFocus").toBool());
        QVERIFY(m_keyboardUrl.isEmpty());
        QTest::keyClick(m_window, Qt::Key_Down); QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("rememberButton")->property("activeFocus").toBool());
        const bool before = object("rememberButton")->property("checked").toBool();
        QTest::keyClick(m_window, Qt::Key_Return);
        QCOMPARE(object("rememberButton")->property("checked").toBool(), !before);
    }
    void settingsWithDpad() {
        m_window->resize(800, 500);
        QTest::keyClick(m_window, Qt::Key_F1);
        QTRY_VERIFY(object("optionsPopup")->property("visible").toBool());
        QTRY_VERIFY(object("modeSetting")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QCOMPARE(m_settings->playbackMode(), "direct");
        QTest::keyClick(m_window, Qt::Key_Down); QTest::keyClick(m_window, Qt::Key_Return);
        QCOMPARE(m_settings->bitrate(), 4000000);
        for (int i = 0; i < 3; ++i) QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("themeVolumeSetting")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Right); QCOMPARE(m_settings->themeSongVolume(), 75);
        Settings saved; QCOMPARE(saved.themeSongVolume(), 75);
        QTest::keyClick(m_window, Qt::Key_Left); QCOMPARE(m_settings->themeSongVolume(), 70);
        QTest::keyClick(m_window, Qt::Key_Up); QTRY_VERIFY(object("themeSongsSetting")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Down);
        for (int i = 0; i < 3; ++i) QTest::keyClick(m_window, Qt::Key_Down);
        auto *accent = qobject_cast<QQuickItem *>(object("accentSetting")); QVERIFY(accent);
        QTRY_VERIFY(accent->hasActiveFocus());
        auto *scroll = qobject_cast<QQuickItem *>(object("settingsScroll")); QVERIFY(scroll);
        QTRY_VERIFY(QRectF(0, 0, scroll->width(), scroll->height()).contains(accent->mapRectToItem(scroll, accent->boundingRect())));
        const auto preview = qEnvironmentVariable("JF_SETTINGS_SCREENSHOT");
        if (!preview.isEmpty()) QVERIFY(m_window->grabWindow().save(preview));
        auto *scale = qobject_cast<QQuickItem *>(object("uiScaleSetting")); QVERIFY(scale);
        scale->forceActiveFocus();
        for (int i = 0; i < 8; ++i) { QTest::keyClick(m_window, Qt::Key_Right); QTest::qWait(10); }
        QCOMPARE(m_settings->uiScale(), 1.4);
        QTRY_VERIFY(QRectF(0, 0, scroll->width(), scroll->height()).adjusted(-1, -1, 1, 1).contains(scale->mapRectToItem(scroll, scale->boundingRect())));
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!object("optionsPopup")->property("visible").toBool());
    }
    void virtualGamepadSelectsAndNavigates() {
        const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        QVERIFY2(index >= 0, SDL_GetError());
        SDL_Joystick *stick = SDL_JoystickOpen(index); QVERIFY(stick);
        QTRY_VERIFY(m_input->name() != "Keyboard");
        auto *server = qobject_cast<QQuickItem *>(object("serverField")); server->forceActiveFocus();
        QTRY_VERIFY(m_window->isActive());
        QCOMPARE(SDL_JoystickSetVirtualButton(stick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1), 0);
        QTRY_VERIFY(object("usernameField")->property("activeFocus").toBool());
        SDL_JoystickSetVirtualButton(stick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0); QTest::qWait(30);
        SDL_JoystickSetVirtualButton(stick, SDL_CONTROLLER_BUTTON_A, 1);
        QTRY_COMPARE(m_keyboardUrl.scheme(), "steam");
        QVERIFY(object("usernameField")->property("activeFocus").toBool());
        SDL_JoystickSetVirtualButton(stick, SDL_CONTROLLER_BUTTON_A, 0);
        SDL_JoystickClose(stick); QCOMPARE(SDL_JoystickDetachVirtual(index), 0);
        QTRY_COMPARE(m_input->name(), "Keyboard");
    }
    void homeBrowseSearchAndPagination() {
        QVERIFY(signIn()); QCOMPARE(m_app->libraries().size(), 1); QCOMPARE(m_app->shelves().size(), 3);
        QCOMPARE(m_app->shelves().first().toMap()["items"].toList().size(), 3);
        QVERIFY(!m_settings->value("accessToken").isValid());
        m_app->home("Movies"); QTRY_VERIFY(!m_app->busy()); QVERIFY(m_app->hasMore());
        QCOMPARE(m_app->shelves().first().toMap()["items"].toList().size(), 60);
        m_app->loadMore(); QTRY_VERIFY(!m_app->busy()); QVERIFY(!m_app->hasMore());
        QCOMPARE(m_app->shelves().first().toMap()["items"].toList().size(), 61);
        m_app->search("ocean"); QTRY_VERIFY(!m_app->busy());
        QCOMPARE(QUrlQuery(m_network->requests.last().request.url()).queryItemValue("SearchTerm"), "ocean");
        m_app->home(); QTRY_VERIFY(!m_app->busy());
        m_app->open(m_app->libraries().first().toMap()); QTRY_VERIFY(!m_app->busy()); QVERIFY(m_app->canGoBack());
        m_app->back(); QTRY_VERIFY(!m_app->busy()); QCOMPARE(m_app->title(), "Home");
        const auto preview = qEnvironmentVariable("JF_TEST_SCREENSHOT");
        if (!preview.isEmpty()) QVERIFY(m_window->grabWindow().save(preview));
    }
    void libraryNavigation() {
        QVERIFY(signIn()); QTest::qWait(20);
        auto *shelf = qobject_cast<QQuickItem *>(object("shelf-0")); QVERIFY(shelf); shelf->forceActiveFocus();
        QTest::keyClick(m_window, Qt::Key_Right);
        QCOMPARE(m_window->property("selectedMedia").toMap()["title"].toString(), "Another good watch 1");
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("shelf-1")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_VERIFY(object("shelf-0")->property("activeFocus").toBool());
    }
    void libraryGridNavigationAndPagination() {
        QVERIFY(signIn());
        m_app->openLibrary(m_app->libraries().first().toMap()); QTRY_VERIFY(!m_app->busy());
        auto *grid = qobject_cast<QQuickItem *>(object("libraryGrid")); QVERIFY(grid);
        QTRY_VERIFY(grid->hasActiveFocus());
        const int columns = grid->property("columns").toInt(); QVERIFY(columns > 1);
        QTRY_COMPARE(object("libraryGridView")->property("count").toInt(), 60);
        auto *first = qobject_cast<QQuickItem *>(object("gridCard-0")); QVERIFY(first);
        auto *secondRow = qobject_cast<QQuickItem *>(object(qPrintable(QString("gridCard-%1").arg(columns)))) ; QVERIFY(secondRow);
        QVERIFY(secondRow->y() > first->y()); QCOMPARE(secondRow->x(), first->x());
        const auto preview = qEnvironmentVariable("JF_GRID_SCREENSHOT");
        if (!preview.isEmpty()) QVERIFY(m_window->grabWindow().save(preview));
        QTest::keyClick(m_window, Qt::Key_Right); QTest::keyClick(m_window, Qt::Key_Down);
        QCOMPARE(grid->property("itemIndex").toInt(), columns + 1);
        for (int i = 0; i < 2; ++i) QTest::keyClick(m_window, Qt::Key_Down);
        const int selected = columns * 3 + 1;
        QCOMPARE(grid->property("itemIndex").toInt(), selected);
        auto *scroll = qobject_cast<QQuickItem *>(object("mediaFlick")); QVERIFY(scroll);
        QTRY_VERIFY(scroll->property("contentY").toDouble() > 0);
        auto *card = qobject_cast<QQuickItem *>(object(qPrintable(QString("gridCard-%1").arg(selected)))); QVERIFY(card);
        const auto inside = [&] { return QRectF(0, 0, scroll->width(), scroll->height()).adjusted(-1, -1, 1, 1).contains(card->mapRectToItem(scroll, card->boundingRect())); };
        QTRY_VERIFY(inside());
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("detailView")->property("visible").toBool()); QTRY_VERIFY(!m_app->detailLoading());
        QCOMPARE(m_app->detail()["id"].toString(), QString("movie-%1").arg(selected));
        QTest::keyClick(m_window, Qt::Key_Escape); QTRY_VERIFY(grid->hasActiveFocus());
        QCOMPARE(grid->property("itemIndex").toInt(), selected);
        m_window->resize(800, 500); QTRY_VERIFY(grid->property("columns").toInt() < columns);
        QTRY_VERIFY(inside());
        m_settings->setUiScale(1.25); QTRY_VERIFY(inside());
        m_settings->setUiScale(1.0);
        for (int i = 0; i < 30 && !object("moreButton")->property("activeFocus").toBool(); ++i) QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("moreButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QTRY_VERIFY(!m_app->busy());
        QTRY_COMPARE(object("libraryGridView")->property("count").toInt(), 61);
        QTRY_VERIFY(grid->hasActiveFocus()); QVERIFY(!m_app->hasMore());
        QTest::keyClick(m_window, Qt::Key_Down); QCOMPARE(grid->property("itemIndex").toInt(), 60);
        for (int i = 0; i < 30 && grid->hasActiveFocus(); ++i) QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_VERIFY(object("filterButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QTRY_VERIFY(!m_app->busy());
        QCOMPARE(m_app->libraryFilter(), "Favorites");
    }
    void fullScreenSeriesAndEpisodeNavigation() {
        const auto original = m_network->respond;
        const QJsonObject series{{"Id", "series"}, {"Name", "A series"}, {"Type", "Series"}, {"IsFolder", true},
            {"Overview", "A crew crosses an unfamiliar ocean in search of a place to call home."}};
        const QJsonArray seasons{
            QJsonObject{{"Id", "season-1"}, {"Name", "Season 1"}, {"Type", "Season"}, {"SeriesId", "series"}, {"IsFolder", true}},
            QJsonObject{{"Id", "season-2"}, {"Name", "Season 2"}, {"Type", "Season"}, {"SeriesId", "series"}, {"IsFolder", true}}};
        auto episode = movie("episode", "The crossing");
        episode["Type"] = "Episode"; episode["SeriesId"] = "series"; episode["SeriesName"] = "A series";
        const auto makeEpisode = [episode](const QString &seasonId, int index) {
            auto item = episode;
            item["Id"] = seasonId + "-episode-" + QString::number(index);
            item["SeasonId"] = seasonId; item["SeasonName"] = "Season " + seasonId.right(1);
            item["ParentIndexNumber"] = seasonId.right(1).toInt(); item["IndexNumber"] = index + 1;
            return item;
        };
        m_network->respond = [=](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.endsWith("/Items/series")) return FixtureResponse{QJsonDocument(series).toJson()};
            const auto id = path.section('/', -1);
            if (path.contains("/Items/") && id.contains("-episode-"))
                return FixtureResponse{QJsonDocument(makeEpisode(id.section("-episode-", 0, 0), id.right(1).toInt())).toJson()};
            if (path.endsWith("/Seasons")) return FixtureResponse{QJsonDocument(QJsonObject{{"Items", seasons}}).toJson()};
            if (path.endsWith("/Episodes")) {
                const auto seasonId = QUrlQuery(request.request.url()).queryItemValue("SeasonId");
                return FixtureResponse{QJsonDocument(QJsonObject{{"Items", QJsonArray{makeEpisode(seasonId, 0), makeEpisode(seasonId, 1)}}}).toJson(), 200, seasonId == "season-1" ? 40 : 0};
            }
            return original(request);
        };
        QVERIFY(signIn());
        const QVariant item = QVariantMap{{"id", "series"}, {"title", "A series"}, {"type", "Series"}, {"folder", true}};
        QVERIFY(QMetaObject::invokeMethod(m_window, "activate", Q_ARG(QVariant, item)));
        auto *view = qobject_cast<QQuickItem *>(object("detailView")); QVERIFY(view);
        QTRY_VERIFY(view->isVisible()); QTRY_VERIFY(object("playButton")->property("activeFocus").toBool());
        QCOMPARE(view->mapRectToScene(view->boundingRect()), QRectF(0, 0, m_window->width(), m_window->height()));
        QVERIFY(!object("libraryScreen")->property("visible").toBool());
        QCOMPARE(object("playButton")->property("text").toString(), "Episodes");
        QTRY_COMPARE(m_app->detail()["seasons"].toList().size(), 2);
        QCOMPARE(object("seasonShelf-0")->property("title").toString(), "Season 1");
        QCOMPARE(object("seasonShelf-1")->property("title").toString(), "Season 2");
        QCOMPARE(m_app->title(), "Home"); QVERIFY(!m_app->canGoBack());
        const auto preview = qEnvironmentVariable("JF_SERIES_SCREENSHOT");
        if (!preview.isEmpty()) QVERIFY(m_window->grabWindow().save(preview));
        QTest::keyClick(m_window, Qt::Key_Down); QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("watchedButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("seasonShelf-0")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_VERIFY(object("playButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("seasonShelf-0")->property("activeFocus").toBool());
        m_window->resize(800, 500);
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("seasonShelf-1")->property("activeFocus").toBool());
        auto *scroll = qobject_cast<QQuickItem *>(object("detailScroll")); QVERIFY(scroll);
        auto *shelf = qobject_cast<QQuickItem *>(object("seasonShelf-1")); QVERIFY(shelf);
        QTRY_VERIFY(QRectF(0, 0, scroll->width(), scroll->height()).contains(shelf->mapRectToItem(scroll, shelf->boundingRect())));
        QTest::keyClick(m_window, Qt::Key_Right);
        QCOMPARE(object("seasonShelf-1")->property("itemIndex").toInt(), 1);
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!m_app->detailLoading());
        QTRY_VERIFY(object("playButton")->property("activeFocus").toBool());
        QCOMPARE(m_app->detail()["id"].toString(), "season-2-episode-1");
        QVERIFY(view->isVisible()); QCOMPARE(m_app->title(), "Home");
        qobject_cast<QQuickItem *>(object("seasonButton"))->forceActiveFocus(); QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_COMPARE(m_app->detail()["type"].toString(), "Series");
        QTRY_VERIFY(object("seasonShelf-1") && object("seasonShelf-1")->property("activeFocus").toBool());
        QCOMPARE(object("seasonShelf-1")->property("itemIndex").toInt(), 1);
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_COMPARE(m_app->detail()["type"].toString(), "Episode");
        QTRY_VERIFY(object("playButton")->property("activeFocus").toBool());
        qobject_cast<QQuickItem *>(object("seriesButton"))->forceActiveFocus(); QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("seasonShelf-0")); QTRY_VERIFY(!m_app->detail()["seasonsLoading"].toBool());
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_COMPARE(m_app->detail()["type"].toString(), "Episode");
        QTRY_VERIFY(object("playButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_COMPARE(m_app->detail()["type"].toString(), "Series");
        QTRY_VERIFY(object("seasonShelf-1") && object("seasonShelf-1")->property("activeFocus").toBool());
        QCOMPARE(object("seasonShelf-1")->property("itemIndex").toInt(), 1);
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!view->isVisible()); QTRY_VERIFY(object("shelf-0")->property("activeFocus").toBool());
        QCOMPARE(m_app->title(), "Home"); QVERIFY(!m_app->canGoBack());
        const QVariant seasonItem = QVariantMap{{"id", "season-2"}, {"type", "Season"}, {"folder", true}, {"seriesId", "series"}, {"seriesName", "A series"}};
        QVERIFY(QMetaObject::invokeMethod(m_window, "activate", Q_ARG(QVariant, seasonItem)));
        QTRY_VERIFY(view->isVisible());
        QTRY_VERIFY(object("seasonShelf-1") && object("seasonShelf-1")->property("activeFocus").toBool());
        QCOMPARE(m_app->title(), "Home"); QVERIFY(!m_app->canGoBack());
        QTest::keyClick(m_window, Qt::Key_Escape); QTRY_VERIFY(!view->isVisible());
    }
    void profilePictureAndThemeSongLifecycle() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QImage image(32, 32, QImage::Format_RGB32); image.fill(QColor("#76dfba"));
        QByteArray png; QBuffer buffer(&png); QVERIFY(buffer.open(QIODevice::WriteOnly)); QVERIFY(image.save(&buffer, "PNG"));
        QByteArray wav; QDataStream stream(&wav, QIODevice::WriteOnly); stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF", 4); stream << quint32(32036); stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4); stream << quint32(32000); wav.append(QByteArray(32000, '\0'));
        QList<QUrl> assets;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, &server, [&, socket] {
                    auto request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
                    socket->setProperty("sent", true);
                    const QUrl url("http://localhost" + QString::fromUtf8(request.split(' ').value(1))); assets.append(url);
                    const bool audio = url.path().endsWith("/Audio/theme/stream");
                    const auto data = audio ? wav : png;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: " + QByteArray(audio ? "audio/wav" : "image/png")
                        + "\r\nContent-Length: " + QByteArray::number(data.size()) + "\r\nConnection: close\r\n\r\n" + data);
                    socket->disconnectFromHost();
                });
            }
        });
        const auto original = m_network->respond;
        const QJsonObject user{{"Id", "user"}, {"Name", "Fixture Viewer"}, {"PrimaryImageTag", "avatar-tag"}};
        m_network->respond = [=](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.endsWith("AuthenticateByName")) return FixtureResponse{QJsonDocument(QJsonObject{{"AccessToken", "fixture-token"}, {"User", user}}).toJson()};
            if (path.endsWith("/Users/Me")) return FixtureResponse{QJsonDocument(user).toJson()};
            if (path.endsWith("/ThemeSongs")) return FixtureResponse{R"({"Items":[{"Id":"theme"}],"OwnerId":"series"})", 200, 40};
            if (path.endsWith("/Items/episode") || path.endsWith("/Items/episode-2")) {
                return FixtureResponse{QJsonDocument(QJsonObject{{"Id", path.section('/', -1)}, {"Name", "An episode"}, {"Type", "Episode"}, {"SeriesId", "series"}}).toJson()};
            }
            if (path.endsWith("/Items/series")) return FixtureResponse{R"({"Id":"series","Name":"A series","Type":"Series","IsFolder":true})"};
            return original(request);
        };
        auto *theme = m_app->findChild<MpvPlayer *>("themePlayer"); QVERIFY(theme);
        QCOMPARE(mpv_set_property_string(theme->core()->handle, "options/ao", "null"), 0);
        const auto base = QString("http://127.0.0.1:%1/base").arg(server.serverPort());
        m_app->login(base, "fixture", "password", true);
        QTRY_VERIFY(m_app->signedIn() && !m_app->busy());
        const QUrl profile(m_app->profileImage());
        QCOMPARE(profile.path(), "/base/Users/user/Images/Primary");
        QCOMPARE(QUrlQuery(profile).queryItemValue("tag"), "avatar-tag");
        QTRY_COMPARE(object("profileImage")->property("status").toInt(), 1);
        QVERIFY(std::any_of(assets.begin(), assets.end(), [](const auto &url) { return url.path().endsWith("/Users/user/Images/Primary"); }));
        qobject_cast<QQuickItem *>(object("profileButton"))->forceActiveFocus(); QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("optionsPopup")->property("visible").toBool());
        QTest::keyClick(m_window, Qt::Key_Escape);
        QSignalSpy loaded(theme, &MpvPlayer::loaded);
        m_app->showDetails({{"id", "episode"}});
        QTRY_VERIFY(!m_app->detailLoading());
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty() && theme->position() > 0.1, 5000);
        QVERIFY(!m_player->active());
        double volume = 0; QCOMPARE(mpv_get_property(theme->core()->handle, "volume", MPV_FORMAT_DOUBLE, &volume), 0); QCOMPARE(volume, 70.0);
        const auto loads = loaded.size();
        m_app->showDetails({{"id", "series"}, {"type", "Series"}});
        QTRY_VERIFY(!m_app->detailLoading()); QTest::qWait(100);
        QVERIFY(theme->active()); QCOMPARE(loaded.size(), loads);
        m_app->showDetails({{"id", "episode-2"}, {"seriesId", "series"}});
        QTRY_VERIFY(!m_app->detailLoading()); QTest::qWait(100);
        QVERIFY(theme->active()); QCOMPARE(loaded.size(), loads);
        m_settings->setThemeSongVolume(90);
        QCOMPARE(mpv_get_property(theme->core()->handle, "volume", MPV_FORMAT_DOUBLE, &volume), 0); QCOMPARE(volume, 90.0);
        QTest::qWait(40); QVERIFY(theme->active()); QCOMPARE(loaded.size(), loads);
        m_settings->setThemeSongVolume(150); QCOMPARE(m_settings->themeSongVolume(), 100);
        m_settings->setThemeSongVolume(-5); QCOMPARE(m_settings->themeSongVolume(), 0);
        m_settings->setThemeSongVolume(85); Settings saved; QCOMPARE(saved.themeSongVolume(), 85);
        QVERIFY(std::any_of(assets.begin(), assets.end(), [](const auto &url) {
            return url.path() == "/base/Audio/theme/stream" && QUrlQuery(url).queryItemValue("api_key") == "fixture-token";
        }));
        auto themeRequest = std::find_if(m_network->requests.cbegin(), m_network->requests.cend(), [](const auto &r) { return r.request.url().path().endsWith("/ThemeSongs"); });
        QVERIFY(themeRequest != m_network->requests.cend());
        QCOMPARE(QUrlQuery(themeRequest->request.url()).queryItemValue("InheritFromParent"), "true");
        m_settings->setThemeSongs(false); QVERIFY(!theme->active());
        m_settings->setThemeSongs(true); QTRY_VERIFY(theme->active());
        m_app->closeDetails(); QVERIFY(!theme->active());
        m_app->showDetails({{"id", "movie"}}); QTRY_VERIFY(!m_app->detailLoading());
        m_app->closeDetails(); QTest::qWait(80); QVERIFY(!theme->active());
        m_app->showDetails({{"id", "movie"}}); QTRY_VERIFY(!m_app->detailLoading());
        m_app->play(m_app->detail()); QTRY_VERIFY(m_player->active()); QTest::qWait(80); QVERIFY(!theme->active());
        m_app->stopPlayback(); m_app->logout(); QVERIFY(m_app->profileImage().isEmpty()); QVERIFY(!theme->active());
        m_settings->put("accessToken", "fixture-token"); m_settings->put("userId", "user"); m_app->start();
        QTRY_VERIFY(m_app->signedIn() && !m_app->busy()); QCOMPARE(m_app->profileImage(), profile.toString());
        const auto withThemes = m_network->respond;
        m_network->respond = [=](const FixtureRequest &request) {
            if (request.request.url().path().endsWith("/ThemeSongs")) return FixtureResponse{"{}", 404};
            return withThemes(request);
        };
        m_app->showDetails({{"id", "movie"}}); QTRY_VERIFY(!m_app->detailLoading());
        QTest::qWait(80); QVERIFY(!theme->active()); QVERIFY(m_app->message().isEmpty());
        m_app->logout();
        m_network->respond = [=](const FixtureRequest &request) {
            if (request.request.url().path().endsWith("AuthenticateByName"))
                return FixtureResponse{R"({"AccessToken":"fixture-token","User":{"Id":"user","Name":"Fixture Viewer"}})"};
            return original(request);
        };
        m_app->login(base, "fixture", "password", false);
        QTRY_VERIFY(m_app->signedIn() && !m_app->busy()); QVERIFY(m_app->profileImage().isEmpty());
        QTRY_VERIFY(object("profileInitial")->property("visible").toBool());
    }
    void blurHashDecodingAndLazyArtwork() {
        const auto black = BlurHashProvider::decode("000000");
        QCOMPARE(black.size(), QSize(32, 24)); QCOMPARE(black.pixelColor(5, 5), QColor(Qt::black));
        const QString sample = "LEHV6nWB2yk8pyo0adR*.7kCMdnj";
        const auto decoded = BlurHashProvider::decode(sample);
        QVERIFY(!decoded.isNull()); QVERIFY(decoded.pixel(0, 0) != decoded.pixel(16, 12));
        for (const auto &invalid : {"", "abc", "00000!", "~00000", "00~~~~", "LEHV6nWB2yk8pyo0adR*.7kCMdn!"})
            QVERIFY(BlurHashProvider::decode(QString::fromLatin1(invalid)).isNull());
        QCOMPARE(BlurHashProvider::decode(sample, QSize(10000, 10000)).size(), QSize(64, 64));
        BlurHashProvider provider; QSize size;
        QCOMPARE(provider.requestImage(QString::fromLatin1(sample.toLatin1().toHex()), &size, QSize(32, 24)), decoded);
        // The full image stays unrequested until its card enters the viewport.
        QTemporaryDir directory; const auto path = directory.filePath("art.png"); QVERIFY(decoded.save(path));
        QQmlComponent component(m_engine.get(), QUrl("qrc:/qml/Artwork.qml"));
        std::unique_ptr<QObject> art(component.createWithInitialProperties({{"load", false}, {"url", QUrl::fromLocalFile(path).toString()}, {"blurHash", sample}}));
        QVERIFY2(art, qPrintable(component.errorString()));
        QCOMPARE(art->property("status").toInt(), 0); // Image.Null
        art->setProperty("load", true);
        QTRY_COMPARE(art->property("status").toInt(), 1); // Image.Ready
        art->setProperty("load", false); QCOMPARE(art->property("status").toInt(), 0);
    }
    void episodeArtworkAndSeriesNavigation() {
        QVERIFY(signIn());
        const QString posterHash = "LEHV6nWB2yk8pyo0adR*.7kCMdnj", backdropHash = "000000";
        QJsonObject episode{{"Id", "episode"}, {"Name", "An episode"}, {"Type", "Episode"},
            {"SeriesId", "series"}, {"SeriesName", "A series"}, {"SeasonId", "season"}, {"SeasonName", "Season 2"},
            {"PrimaryImageAspectRatio", 16.0 / 9}, {"ImageTags", QJsonObject{{"Primary", "episode-tag"}}},
            {"SeriesPrimaryImageTag", "series-tag"}, {"ParentBackdropItemId", "series"}, {"ParentBackdropImageTags", QJsonArray{"backdrop-tag"}},
            {"ImageBlurHashes", QJsonObject{{"Primary", QJsonObject{{"episode-tag", posterHash}, {"series-tag", backdropHash}}}, {"Backdrop", QJsonObject{{"backdrop-tag", backdropHash}}}}}};
        const auto original = m_network->respond;
        m_network->respond = [=](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.endsWith("/Items/episode")) return FixtureResponse{QJsonDocument(episode).toJson()};
            if (path.endsWith("/Items/series")) return FixtureResponse{R"({"Id":"series","Name":"A series","Type":"Series","IsFolder":true})"};
            if (path.endsWith("/Seasons")) return FixtureResponse{R"({"Items":[{"Id":"season","Name":"Season 2","Type":"Season","SeriesId":"series","IsFolder":true}]})"};
            if (path.endsWith("/Episodes")) return FixtureResponse{QJsonDocument(QJsonObject{{"Items", QJsonArray{episode}}, {"TotalRecordCount", 1}}).toJson()};
            return original(request);
        };
        m_app->showDetails({{"id", "episode"}}); QTRY_VERIFY(!m_app->detailLoading());
        auto detail = m_app->detail();
        QVERIFY(detail["poster"].toString().contains("/Items/episode/Images/Primary"));
        QVERIFY(detail["backdrop"].toString().contains("/Items/series/Images/Backdrop/0"));
        QCOMPARE(detail["posterBlurHash"].toString(), posterHash); QCOMPARE(detail["backdropBlurHash"].toString(), backdropHash);
        QCOMPARE(detail["imageAspect"].toDouble(), 16.0 / 9);
        m_app->openSeries(detail);
        QTRY_VERIFY(!m_app->detailLoading() && !m_app->detail()["seasonsLoading"].toBool());
        QCOMPARE(m_app->detail()["title"].toString(), "A series");
        const auto sections = m_app->detail()["seasons"].toList(); QCOMPARE(sections.size(), 1);
        QCOMPARE(sections.first().toMap()["title"].toString(), "Season 2");
        QCOMPARE(sections.first().toMap()["items"].toList().first().toMap()["id"].toString(), "episode");
        QCOMPARE(m_network->requests.last().request.url().path(), "/base/Shows/series/Episodes");
        const QUrlQuery episodeQuery(m_network->requests.last().request.url());
        QCOMPARE(episodeQuery.queryItemValue("SeasonId"), "season");
        QVERIFY(!episodeQuery.hasQueryItem("Limit"));
        QCOMPARE(m_app->title(), "Home"); QVERIFY(!m_app->canGoBack());
        episode.remove("ImageTags");
        m_network->respond = [=](const FixtureRequest &) { return FixtureResponse{QJsonDocument(episode).toJson()}; };
        m_app->showDetails({{"id", "episode"}}); QTRY_VERIFY(!m_app->detailLoading());
        QVERIFY(m_app->detail()["poster"].toString().contains("/Items/series/Images/Primary"));
        QCOMPARE(m_app->detail()["posterBlurHash"].toString(), backdropHash);
        QCOMPARE(m_app->detail()["imageAspect"].toDouble(), 2.0 / 3);
    }
    void favoritesWatchedFiltersAndErrors() {
        QVERIFY(signIn());
        auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        m_app->showDetails(item); QTRY_VERIFY(!m_app->detailLoading());
        auto *popup = object("detailView"); QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(object("playButton")->property("activeFocus").toBool());
        QVERIFY(!object("preplayQuality")->property("visible").toBool());
        const auto preview = qEnvironmentVariable("JF_COMPACT_DETAIL_SCREENSHOT");
        if (!preview.isEmpty()) QVERIFY(m_window->grabWindow().save(preview));
        QTest::keyClick(m_window, Qt::Key_Down); QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("favoriteButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QTRY_VERIFY(!m_app->updatingUserData());
        QVERIFY(m_app->detail()["favorite"].toBool());
        QVERIFY(object("favoriteButton")->property("checked").toBool());
        QVERIFY(m_app->shelves().first().toMap()["items"].toList().first().toMap()["favorite"].toBool());
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_VERIFY(object("watchedButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QTRY_VERIFY(!m_app->updatingUserData());
        QVERIFY(m_app->detail()["played"].toBool()); QCOMPARE(m_app->detail()["progress"].toDouble(), 0.0);
        QVERIFY(object("watchedButton")->property("checked").toBool());
        m_app->setFavorite(m_app->detail(), false); QTRY_VERIFY(!m_app->updatingUserData());
        QVERIFY(!m_app->detail()["favorite"].toBool());
        QCOMPARE(m_network->requests.last().operation, QNetworkAccessManager::DeleteOperation);
        m_app->setPlayed(m_app->detail(), false); QTRY_VERIFY(!m_app->updatingUserData());
        QVERIFY(!m_app->detail()["played"].toBool());
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!popup->property("visible").toBool());
        m_app->home("Movies"); QTRY_VERIFY(!m_app->busy());
        m_app->setLibraryFilter("Favorites"); QTRY_VERIFY(!m_app->busy());
        QCOMPARE(QUrlQuery(m_network->requests.last().request.url()).queryItemValue("Filters"), "IsFavorite");
        m_app->setSortOrder("Recently added"); QTRY_VERIFY(!m_app->busy());
        QCOMPARE(QUrlQuery(m_network->requests.last().request.url()).queryItemValue("SortBy"), "DateCreated");
        m_app->setLibraryFilter("Unwatched"); QTRY_VERIFY(!m_app->busy());
        QCOMPARE(QUrlQuery(m_network->requests.last().request.url()).queryItemValue("IsPlayed"), "false");
        m_app->showDetails(item); QTRY_VERIFY(!m_app->detailLoading());
        m_network->respond = [](const FixtureRequest &) { return FixtureResponse{"{}", 500}; };
        m_app->setFavorite(m_app->detail(), true); QTRY_VERIFY(!m_app->updatingUserData());
        QVERIFY(!m_app->detail()["favorite"].toBool()); QVERIFY(m_app->message().contains("500"));
    }
    void staleDetailsCannotReplaceCurrentTitle() {
        QVERIFY(signIn());
        m_network->respond = [this](const FixtureRequest &request) {
            const auto id = request.request.url().path().section('/', -1);
            return FixtureResponse{QJsonDocument(movie(id)).toJson(), 200, id == "old" ? 80 : 0};
        };
        m_app->showDetails({{"id", "old"}}); m_app->showDetails({{"id", "new"}});
        QTRY_VERIFY(!m_app->detailLoading()); QCOMPARE(m_app->detail()["id"].toString(), "new");
        QTest::qWait(100); QCOMPARE(m_app->detail()["id"].toString(), "new");
        m_app->showDetails({{"id", "old"}}); m_app->closeDetails(); QTest::qWait(100);
        QVERIFY(m_app->detail().isEmpty()); QVERIFY(!m_app->detailLoading());
    }
    void seasonSectionsHandleErrorsAndStaleReplies() {
        QVERIFY(signIn());
        const auto original = m_network->respond;
        auto mode = std::make_shared<int>(0);
        m_network->respond = [=, this](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.endsWith("/Items/series")) return FixtureResponse{R"({"Id":"series","Name":"A series","Type":"Series","IsFolder":true})"};
            if (path.endsWith("/Seasons")) {
                if (*mode == 0) return FixtureResponse{R"({"Items":[]})"};
                if (*mode == 1) return FixtureResponse{"{}", 503};
                return FixtureResponse{R"({"Items":[{"Id":"failed","Name":"Specials"},{"Id":"empty","Name":"Season 1"},{"Id":"full","Name":"Season 2"}]})"};
            }
            if (path.endsWith("/Episodes")) {
                const auto id = QUrlQuery(request.request.url()).queryItemValue("SeasonId");
                if (id == "failed") return FixtureResponse{"{}", 500};
                QJsonArray episodes;
                if (id == "full") for (int i = 0; i < 61; ++i) {
                    auto item = movie(QString("episode-%1").arg(i)); item["Type"] = "Episode"; episodes.append(item);
                }
                return FixtureResponse{QJsonDocument(QJsonObject{{"Items", episodes}}).toJson(), 200, 80};
            }
            return original(request);
        };
        const QVariantMap series{{"id", "series"}, {"type", "Series"}, {"folder", true}};
        m_app->showDetails(series);
        QTRY_VERIFY(!m_app->detailLoading() && !m_app->detail()["seasonsLoading"].toBool());
        QVERIFY(m_app->detail()["seasons"].toList().isEmpty());
        *mode = 1; m_app->showDetails(series);
        QTRY_VERIFY(!m_app->detailLoading() && !m_app->detail()["seasonsLoading"].toBool());
        QVERIFY(m_app->detail()["seasonsError"].toString().contains("503"));
        *mode = 2; m_app->showDetails(series);
        QTRY_VERIFY(!m_app->detailLoading() && !m_app->detail()["seasonsLoading"].toBool());
        const auto sections = m_app->detail()["seasons"].toList(); QCOMPARE(sections.size(), 3);
        QVERIFY(sections[0].toMap()["error"].toString().contains("500"));
        QVERIFY(sections[1].toMap()["items"].toList().isEmpty());
        QCOMPARE(sections[2].toMap()["items"].toList().size(), 61);
        const auto episodeRequests = [this] {
            return std::count_if(m_network->requests.cbegin(), m_network->requests.cend(), [](const auto &r) { return r.request.url().path().endsWith("/Episodes"); });
        };
        auto count = episodeRequests();
        m_app->showDetails(series); QTRY_COMPARE(episodeRequests(), count + 3);
        m_app->showDetails({{"id", "movie"}}); QTRY_VERIFY(!m_app->detailLoading());
        QTest::qWait(100);
        QCOMPARE(m_app->detail()["id"].toString(), "movie"); QVERIFY(!m_app->detail().contains("seasons"));
        count = episodeRequests();
        m_app->showDetails(series); QTRY_COMPARE(episodeRequests(), count + 3);
        m_app->closeDetails(); QTest::qWait(100);
        QVERIFY(m_app->detail().isEmpty()); QVERIFY(!m_app->detailLoading());
    }
    void longDetailsStayInsideSmallWindow() {
        QVERIFY(signIn());
        const QString longTitle = QString("A very long movie title with a distant destination ").repeated(8);
        m_network->respond = [this, longTitle](const FixtureRequest &request) {
            auto result = movie(request.request.url().path().section('/', -1), longTitle);
            result["Overview"] = QString("An extended description of the journey. ").repeated(30);
            result["People"] = QJsonArray{QJsonObject{{"Name", "A performer"}, {"Role", "The traveler"}}};
            result["Chapters"] = QJsonArray{QJsonObject{{"Name", "The crossing"}, {"StartPositionTicks", 1200000000}}};
            return FixtureResponse{QJsonDocument(result).toJson()};
        };
        m_window->resize(800, 500);
        m_app->showDetails({{"id", "long"}});
        auto *popup = object("detailView"); QVERIFY(popup); QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(!m_app->detailLoading());
        auto *play = qobject_cast<QQuickItem *>(object("playButton")); QVERIFY(play);
        QTRY_VERIFY(play->hasActiveFocus());
        auto *scroll = qobject_cast<QQuickItem *>(object("detailScroll")); QVERIFY(scroll);
        const auto insideScroll = [scroll](QQuickItem *item) {
            return QRectF(0, 0, scroll->width(), scroll->height()).adjusted(-1, -1, 1, 1).contains(item->mapRectToItem(scroll, item->boundingRect()));
        };
        QTRY_VERIFY(insideScroll(play));
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("restartButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("favoriteButton")->property("activeFocus").toBool());
        QTRY_VERIFY(insideScroll(qobject_cast<QQuickItem *>(object("favoriteButton"))));
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("watchedButton")->property("activeFocus").toBool());
        QTRY_VERIFY(insideScroll(qobject_cast<QQuickItem *>(object("watchedButton"))));
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("preplaySettingsButton")->property("activeFocus").toBool());
        QVERIFY(!object("preplayQuality")->property("visible").toBool());
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("preplayQuality")->property("visible").toBool());
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("preplayQuality")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("preplayAudio")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("preplaySubtitle")->property("activeFocus").toBool());
        QTRY_VERIFY(insideScroll(qobject_cast<QQuickItem *>(object("preplaySubtitle"))));
        QTest::keyClick(m_window, Qt::Key_Up); QTest::keyClick(m_window, Qt::Key_Up); QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_VERIFY(object("preplaySettingsButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!object("preplayQuality")->property("visible").toBool());
        QTRY_VERIFY(insideScroll(qobject_cast<QQuickItem *>(object("preplaySettingsButton"))));
        const auto preview = qEnvironmentVariable("JF_DETAIL_SCREENSHOT");
        if (!preview.isEmpty()) QVERIFY(m_window->grabWindow().save(preview));
        auto *chapter = qobject_cast<QQuickItem *>(object("chapterButton-0")); QVERIFY(chapter);
        QVERIFY(!chapter->isVisible());
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("detailExtrasButton")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QTRY_VERIFY(chapter->isVisible());
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(chapter->hasActiveFocus());
        QTRY_VERIFY(insideScroll(chapter));
        auto *close = qobject_cast<QQuickItem *>(object("detailBackButton")); QVERIFY(close);
        QVERIFY(QRectF(0, 0, m_window->width(), m_window->height()).contains(close->mapRectToScene(close->boundingRect())));
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!popup->property("visible").toBool());
    }
    void negotiatesPlaybackWithoutResettingShelves() {
        QVERIFY(signIn());
        const auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        m_engine.reset(); // Negotiation requires no display; the renderer is tested separately.
        QSignalSpy shelfChanges(m_app.get(), &AppController::shelvesChanged);
        m_app->play(item); QTRY_VERIFY(m_player->active()); QCOMPARE(m_app->playMethod(), "DirectPlay");
        QCOMPARE(shelfChanges.count(), 0);
        m_app->stopPlayback(); QVERIFY(!m_player->active());
        m_settings->setPlaybackMode("transcode"); m_app->play(item); QTRY_VERIFY(m_player->active());
        QCOMPARE(m_app->playMethod(), "Transcode");
        m_app->stopPlayback();
        QVERIFY(std::any_of(m_network->requests.begin(), m_network->requests.end(), [](const auto &r) {
            return r.operation == QNetworkAccessManager::DeleteOperation && r.request.url().path().endsWith("ActiveEncodings");
        }));
    }
    void playbackEventsKeepOptionsFocused() {
        QVERIFY(signIn());
        const auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        m_app->play(item); QTRY_VERIFY(m_player->active());
        QTest::keyClick(m_window, Qt::Key_F1);
        QTRY_VERIFY(object("optionsPopup")->property("visible").toBool());
        QTRY_VERIFY(object("modeSetting")->property("activeFocus").toBool());
        m_player->togglePause(); QTRY_VERIFY(m_player->paused());
        QVERIFY(object("modeSetting")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Escape); m_app->stopPlayback();
    }
    void homeBackPromptsExit() {
        QVERIFY(signIn()); QTest::qWait(20);
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(object("exitPrompt")->property("visible").toBool());
        QVERIFY(!object("optionsPopup")->property("visible").toBool());
        QTRY_VERIFY(object("cancelExit")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!object("exitPrompt")->property("visible").toBool());
        QVERIFY(m_window->isVisible());
    }
    void librariesUseServerNamesOrderAndParents() {
        const auto original = m_network->respond;
        m_network->respond = [original](const FixtureRequest &request) {
            if (request.request.url().path().endsWith("/Views")) return FixtureResponse{R"({"Items":[
                {"Id":"anime","Name":"Anime","Type":"CollectionFolder","CollectionType":"tvshows","IsFolder":true},
                {"Id":"films","Name":"Cinema","Type":"CollectionFolder","CollectionType":"movies","IsFolder":true}]})"};
            return original(request);
        };
        QVERIFY(signIn()); QCOMPARE(m_app->libraries().size(), 2); QCOMPARE(m_app->shelves().size(), 4);
        QCOMPARE(object("libraryTab-0")->property("text").toString(), "Anime");
        QCOMPARE(object("libraryTab-1")->property("text").toString(), "Cinema");
        bool series = false, movies = false;
        for (const auto &request : m_network->requests) {
            const QUrlQuery query(request.request.url());
            if (query.queryItemValue("ParentId") == "anime") series = query.queryItemValue("IncludeItemTypes") == "Series";
            if (query.queryItemValue("ParentId") == "films") movies = query.queryItemValue("IncludeItemTypes") == "Movie";
        }
        QVERIFY(series); QVERIFY(movies);
        m_app->home("Movies"); m_app->home(); QTRY_VERIFY(!m_app->busy());
        QCOMPARE(m_app->shelves().size(), 4);
    }
    void playbackSelectionsSegmentsAndCancellation() {
        QVERIFY(signIn()); m_engine.reset();
        const auto original = m_network->respond;
        m_network->respond = [original](const FixtureRequest &request) {
            if (request.request.url().path().contains("MediaSegments/"))
                return FixtureResponse{R"({"Items":[{"Type":"Intro","StartTicks":0,"EndTicks":300000000}]})", 200, 20};
            return original(request);
        };
        const auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        m_app->play(item, false, {{"quality", 8000000}, {"audio", 2}, {"subtitle", -1}, {"sourceId", "source"}});
        QTRY_VERIFY(m_player->active()); QCOMPARE(m_app->playMethod(), "Transcode");
        QTRY_COMPARE(m_app->mediaSegments().size(), 1);
        auto playbackRequest = [this] {
            for (auto i = m_network->requests.crbegin(); i != m_network->requests.crend(); ++i)
                if (i->request.url().path().endsWith("PlaybackInfo")) return QJsonDocument::fromJson(i->body).object();
            return QJsonObject{};
        };
        auto request = playbackRequest();
        QCOMPARE(request["MaxStreamingBitrate"].toInt(), 8000000);
        QCOMPARE(request["AudioStreamIndex"].toInt(), 2); QCOMPARE(request["SubtitleStreamIndex"].toInt(), -1);
        QCOMPARE(request["MediaSourceId"].toString(), "source");
        QVERIFY(!request["EnableDirectPlay"].toBool());
        m_app->skipSegment(); QTRY_VERIFY(!m_app->playbackPending());
        QCOMPARE(playbackRequest()["StartTimeTicks"].toInteger(), 300000000LL);
        m_app->setQuality(-1); QTRY_VERIFY(!m_app->playbackPending());
        QCOMPARE(m_app->playMethod(), "DirectPlay");
        QCOMPARE(playbackRequest()["StartTimeTicks"].toInteger(), 300000000LL);
        m_player->togglePause(); QTRY_VERIFY(m_player->paused());
        m_app->setQuality(4000000); QTRY_VERIFY(!m_app->playbackPending());
        m_player->loaded(); QTRY_VERIFY(m_player->paused());
        m_app->setPlaybackTrack(false, 1); QTRY_VERIFY(!m_app->playbackPending());
        QCOMPARE(playbackRequest()["AudioStreamIndex"].toInt(), 1);
        QCOMPARE(playbackRequest()["StartTimeTicks"].toInteger(), 300000000LL);
        m_player->loaded(); QTRY_VERIFY(m_player->paused());
        m_app->stopPlayback(); QVERIFY(m_app->mediaSegments().isEmpty());
        m_app->play(item); m_app->stopPlayback(); QTest::qWait(40);
        QVERIFY(m_app->mediaSegments().isEmpty()); QVERIFY(!m_player->active());
        m_network->respond = [original](const FixtureRequest &request) {
            if (request.request.url().path().contains("MediaSegments/")) return FixtureResponse{"{}", 404};
            return original(request);
        };
        m_app->play(item); QTRY_VERIFY(m_player->active());
        QVERIFY(m_app->message().isEmpty()); QVERIFY(m_app->mediaSegments().isEmpty());
        m_app->stopPlayback();
    }
    void timelineChaptersAndSegments_data() {
        QTest::addColumn<QSize>("windowSize");
        QTest::newRow("compact") << QSize(800, 500);
        QTest::newRow("deck") << QSize(1280, 800);
        QTest::newRow("desktop") << QSize(1920, 1080);
    }
    void timelineChaptersAndSegments() {
        QFETCH(QSize, windowSize);
        QVERIFY(signIn()); m_window->resize(windowSize);
        const auto original = m_network->respond;
        m_network->respond = [original](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.contains("MediaSegments/")) return FixtureResponse{R"({"Items":[
                {"Type":"Recap","StartTicks":0,"EndTicks":100000000},
                {"Type":"Intro","StartTicks":100000000,"EndTicks":300000000},
                {"Type":"Intro","StartTicks":500000000,"EndTicks":550000000},
                {"Type":"Outro","StartTicks":1100000000,"EndTicks":1600000000},
                {"Type":"Preview","StartTicks":1300000000,"EndTicks":1400000000}]})"};
            if (path.endsWith("/Items/movie-0")) return FixtureResponse{R"({"Id":"movie-0","Chapters":[
                {"Name":"Finale","StartPositionTicks":600000000},
                {"Name":"Opening","StartPositionTicks":0},
                {"Name":"Middle","StartPositionTicks":300000000},
                {"Name":"Invalid","StartPositionTicks":-1},
                {"Name":"Outside runtime","StartPositionTicks":1500000000},
                {"Name":"Missing position"}]})", 200, 20};
            return original(request);
        };
        auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        item["runtime"] = 120;
        QVERIFY(item["chapters"].toList().isEmpty());
        m_app->play(item, false, {{"quality", 8000000}});
        QTRY_VERIFY(m_player->active());
        QTRY_VERIFY(object("timelineChapter-2"));
        QVERIFY(!object("timelineChapter-3"));
        QVERIFY(object("timelineSegment-3")); QVERIFY(!object("timelineSegment-4"));
        auto *bar = qobject_cast<QQuickItem *>(object("playerSeekBar")); QVERIFY(bar);
        auto *track = qobject_cast<QQuickItem *>(object("timelineTrack")); QVERIFY(track);
        auto *middle = qobject_cast<QQuickItem *>(object("timelineChapter-1")); QVERIFY(middle);
        auto *intro = qobject_cast<QQuickItem *>(object("timelineSegment-1")); QVERIFY(intro);
        auto *credits = qobject_cast<QQuickItem *>(object("timelineSegment-3")); QVERIFY(credits);
        QTRY_VERIFY(track->width() > 0);
        QVERIFY(qAbs(middle->x() + middle->width() / 2 - track->width() / 4) < 0.01);
        QVERIFY(qAbs(intro->x() - track->width() / 12) < 0.01);
        QVERIFY(qAbs(intro->width() - track->width() / 6) < 0.01);
        QVERIFY(qAbs(credits->x() + credits->width() - track->width()) < 0.01);
        QVERIFY(intro->property("color") != credits->property("color"));
        auto *legend = qobject_cast<QQuickItem *>(object("timelineLegend")); QVERIFY(legend);
        QVERIFY(legend->isVisible());
        QTRY_VERIFY(legend->height() > 0);
        QVERIFY(legend->y() + legend->height() <= bar->height());
        bool requestedChapters = false;
        for (const auto &request : m_network->requests) {
            if (request.request.url().path() != "/base/Items/movie-0") continue;
            requestedChapters = request.operation == QNetworkAccessManager::GetOperation &&
                QUrlQuery(request.request.url()).queryItemValue("UserId") == "user" &&
                request.request.rawHeader("Authorization").contains("fixture-token");
        }
        QVERIFY(requestedChapters);
        bar->forceActiveFocus(); QTest::keyClick(m_window, Qt::Key_Return);
        QVERIFY(bar->property("seeking").toBool());
        const auto requestsBefore = m_network->requests.size();
        bar->setProperty("previewPosition", 20);
        QTRY_VERIFY(object("seekPreviewLabel")->property("text").toString().contains("Opening · Intro"));
        bar->setProperty("previewPosition", 30);
        QTRY_COMPARE(bar->property("contextLabel").toString(), "Middle");
        bar->setProperty("previewPosition", 115);
        QTRY_COMPARE(bar->property("contextLabel").toString(), "Finale · Credits");
        auto *preview = qobject_cast<QQuickItem *>(object("trickplayPreview")); QVERIFY(preview);
        QVERIFY(QRectF(0, 0, m_window->width(), m_window->height()).contains(preview->mapRectToScene(preview->boundingRect())));
        const auto screenshot = qEnvironmentVariable("SOFAFIN_TIMELINE_SCREENSHOT");
        if (!screenshot.isEmpty() && windowSize == QSize(1280, 800)) QVERIFY(m_window->grabWindow().save(screenshot));
        QCOMPARE(m_network->requests.size(), requestsBefore);
        bar->setProperty("previewPosition", 20);
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!m_app->playbackPending()); QCOMPARE(m_player->position(), 20.0);
        QVERIFY(object("skipSegmentButton")->property("visible").toBool());
        auto *view = qobject_cast<QQuickItem *>(object("playbackView")); QVERIFY(view);
        QVERIFY(QMetaObject::invokeMethod(view, "focusVideo"));
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("skipSegmentButton")->property("activeFocus").toBool());
        QCOMPARE(object("skipSegmentButton")->property("text").toString(), "Skip intro");
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!m_app->playbackPending()); QCOMPARE(m_player->position(), 30.0);
        QVERIFY(object("timelineChapter-2")); QVERIFY(object("timelineSegment-3"));
        m_app->stopPlayback();
        QVERIFY(!object("timelineChapter-0")); QVERIFY(!object("timelineSegment-0"));
        auto next = item; next["id"] = "next";
        m_network->respond = [original](const FixtureRequest &request) {
            if (request.request.url().path().contains("MediaSegments/")) return FixtureResponse{"{}", 404};
            return original(request);
        };
        m_app->play(next, false); QTRY_VERIFY(m_player->active());
        QVERIFY(!legend->isVisible()); QVERIFY(m_app->message().isEmpty());
        QVERIFY(!object("timelineChapter-0")); QVERIFY(!object("timelineSegment-0"));
        m_app->stopPlayback();
    }
    void stalePlaybackMarkersAreIgnored() {
        QVERIFY(signIn());
        const auto original = m_network->respond;
        m_network->respond = [original](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.endsWith("/Items/old")) return FixtureResponse{R"({"Id":"old","Chapters":[{"Name":"Old chapter","StartPositionTicks":0}]})", 200, 100};
            if (path.endsWith("/MediaSegments/old")) return FixtureResponse{R"({"Items":[{"Type":"Intro","StartTicks":0,"EndTicks":300000000}]})", 200, 100};
            if (path.endsWith("/Items/new")) return FixtureResponse{R"({"Id":"new","Chapters":[{"Name":"New chapter","StartPositionTicks":0}]})"};
            return original(request);
        };
        auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        item["id"] = "old"; m_app->play(item, false); QTRY_VERIFY(m_player->active());
        item["id"] = "new"; m_app->play(item, false); QTRY_VERIFY(!m_app->playbackPending());
        QTRY_COMPARE(m_app->currentItem()["chapters"].toList().size(), 1);
        QTest::qWait(120);
        QCOMPARE(m_app->currentItem()["chapters"].toList().first().toMap()["name"].toString(), "New chapter");
        QVERIFY(m_app->mediaSegments().isEmpty());
        m_app->stopPlayback();
        item["id"] = "old"; m_app->play(item, false); m_app->stopPlayback(); QTest::qWait(120);
        QVERIFY(m_app->currentItem()["chapters"].toList().isEmpty());
        QVERIFY(m_app->mediaSegments().isEmpty());
    }
    void chapterFallbackAndNativeSegmentPriority_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<int>("delay"); QTest::addColumn<bool>("native");
        QTest::newRow("empty-api-before-chapters") << 200 << 0 << false;
        QTest::newRow("empty-api-after-chapters") << 200 << 60 << false;
        QTest::newRow("unsupported-api") << 404 << 60 << false;
        QTest::newRow("native-before-chapters") << 200 << 0 << true;
        QTest::newRow("native-after-chapters") << 200 << 60 << true;
    }
    void chapterFallbackAndNativeSegmentPriority() {
        QFETCH(int, status); QFETCH(int, delay); QFETCH(bool, native);
        QVERIFY(signIn());
        const auto original = m_network->respond;
        m_network->respond = [original, status, delay, native](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.contains("MediaSegments/")) return FixtureResponse{native
                ? QByteArray(R"({"Items":[{"Type":"Intro","StartTicks":500000000,"EndTicks":1500000000}]})")
                : QByteArray(R"({"Items":[]})"), status, delay};
            if (path.endsWith("/Items/movie-0")) return FixtureResponse{R"({"Id":"movie-0","Chapters":[
                {"Name":"Prologue","StartPositionTicks":0},
                {"Name":"Opening","StartPositionTicks":570390000},
                {"Name":"Part A","StartPositionTicks":1469990000},
                {"Name":"Outro","StartPositionTicks":12757090000}]})", 200, 20};
            return original(request);
        };
        auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        item["runtime"] = 1440; item["positionTicks"] = 600000000;
        m_app->play(item, true, {{"quality", 8000000}});
        QTRY_VERIFY(m_player->active()); QTRY_VERIFY(object("timelineChapter-3"));
        QTest::qWait(80);
        QCOMPARE(m_app->mediaSegments().size(), native ? 1 : 2);
        QCOMPARE(m_app->mediaSegments().first().toMap()["end"].toDouble(), native ? 150.0 : 146.999);
        auto *button = object("skipSegmentButton"); QVERIFY(button);
        QVERIFY(button->property("visible").toBool()); QCOMPARE(button->property("text").toString(), "Skip intro");
        auto *view = object("playbackView"); QVERIFY(QMetaObject::invokeMethod(view, "hideOverlay"));
        QVERIFY(button->property("visible").toBool()); // Skip remains available when the controls hide.
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(button->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!m_app->playbackPending()); QCOMPARE(m_player->position(), native ? 150.0 : 146.999);
        QVERIFY(!button->property("visible").toBool());
        QVERIFY(m_app->message().isEmpty()); m_app->stopPlayback();
    }
    void allSegmentTypesShowSkipButtons_data() {
        QTest::addColumn<QString>("type"); QTest::addColumn<QString>("label");
        QTest::newRow("intro") << "Intro" << "Skip intro";
        QTest::newRow("credits") << "Outro" << "Skip credits";
        QTest::newRow("recap") << "Recap" << "Skip recap";
        QTest::newRow("preview") << "Preview" << "Skip preview";
        QTest::newRow("commercial") << "Commercial" << "Skip commercial";
    }
    void allSegmentTypesShowSkipButtons() {
        QFETCH(QString, type); QFETCH(QString, label);
        QVERIFY(signIn());
        const auto original = m_network->respond;
        m_network->respond = [original, type](const FixtureRequest &request) {
            if (request.request.url().path().contains("MediaSegments/")) return FixtureResponse{
                QJsonDocument(QJsonObject{{"Items", QJsonArray{QJsonObject{{"Type", type}, {"StartTicks", 0}, {"EndTicks", 300000000}}}}}).toJson()};
            return original(request);
        };
        const auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        m_app->play(item, false, {{"quality", 8000000}}); QTRY_VERIFY(m_player->active());
        QTRY_COMPARE(m_app->mediaSegments().size(), 1);
        auto *button = object("skipSegmentButton"); QVERIFY(button);
        QVERIFY(button->property("visible").toBool()); QCOMPARE(button->property("text").toString(), label);
        auto *view = object("playbackView"); QVERIFY(QMetaObject::invokeMethod(view, "focusVideo"));
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(button->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!m_app->playbackPending()); QCOMPARE(m_player->position(), 30.0);
        QVERIFY(!button->property("visible").toBool()); m_app->stopPlayback();
    }
    void skippingFinalCreditsStopsPlayback() {
        QVERIFY(signIn());
        auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        item["runtime"] = 120; item["positionTicks"] = 950000000;
        item["chapters"] = QVariantList{QVariantMap{{"name", "Credits"}, {"ticks", 900000000}}};
        m_app->play(item, true, {{"quality", 8000000}}); QTRY_VERIFY(m_player->active());
        QVERIFY(object("skipSegmentButton")->property("visible").toBool());
        QCOMPARE(object("skipSegmentButton")->property("text").toString(), "Skip credits");
        m_app->skipSegment();
        QVERIFY(!m_player->active()); QVERIFY(!m_app->playbackPending()); QVERIFY(m_app->mediaSegments().isEmpty());
    }
    void nextEpisodeCountdownAndTerminalSegments_data() {
        QTest::addColumn<double>("start"); QTest::addColumn<double>("end");
        QTest::addColumn<bool>("chapter"); QTest::addColumn<double>("promptStart");
        QTest::newRow("last-thirty-seconds") << -1.0 << -1.0 << false << 90.0;
        QTest::newRow("final-server-segment") << 60.0 << 120.0 << false << 60.0;
        QTest::newRow("rounded-server-end") << 60.0 << 120.4 << false << 60.0;
        QTest::newRow("final-chapter-segment") << 60.0 << 120.0 << true << 60.0;
        QTest::newRow("post-credits-content") << 60.0 << 100.0 << false << 90.0;
        QTest::newRow("short-final-segment") << 100.0 << 120.0 << false << 100.0;
    }
    void nextEpisodeCountdownAndTerminalSegments() {
        QFETCH(double, start); QFETCH(double, end); QFETCH(bool, chapter); QFETCH(double, promptStart);
        QVERIFY(signIn()); serveEpisodeSequence(); m_window->resize(800, 500);
        const auto original = m_network->respond;
        m_network->respond = [this, original, start, end, chapter](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.contains("MediaSegments/")) {
                QJsonArray segments;
                if (start >= 0 && !chapter) segments.append(QJsonObject{{"Type", "Outro"},
                    {"StartTicks", start * 10000000}, {"EndTicks", end * 10000000}});
                return FixtureResponse{QJsonDocument(QJsonObject{{"Items", segments}}).toJson()};
            }
            if (chapter && path.endsWith("/Items/episode-1")) {
                auto item = episode("episode-1");
                item["Chapters"] = QJsonArray{QJsonObject{{"Name", "Credits"}, {"StartPositionTicks", start * 10000000}}};
                return FixtureResponse{QJsonDocument(item).toJson()};
            }
            return original(request);
        };
        m_app->play(playingEpisode(promptStart - 0.25), true, {{"quality", 8000000}});
        QTRY_VERIFY(m_player->active()); QTRY_VERIFY(m_app->autoplayNextEpisode());
        if (start >= 0) QTRY_COMPARE(m_app->mediaSegments().size(), 1);
        auto *prompt = qobject_cast<QQuickItem *>(object("nextEpisodePrompt")); QVERIFY(prompt);
        QVERIFY(!prompt->isVisible());
        m_app->seekRelative(0.25); QTRY_VERIFY(!m_app->playbackPending());
        QTRY_VERIFY(prompt->isVisible());
        QCOMPARE(object("nextEpisodeCountdown")->property("text").toString(),
            QString("Next Episode in %1 seconds").arg(qCeil(120 - promptStart)));
        QCOMPARE(object("nextEpisodeInfo")->property("text").toString(), "S2 · E1  Episode 2");
        QVERIFY(!object("skipSegmentButton")->property("visible").toBool());
        QTRY_VERIFY(QRectF(0, 0, m_window->width(), m_window->height()).contains(prompt->mapRectToScene(prompt->boundingRect())));
        QVERIFY(QMetaObject::invokeMethod(object("playbackView"), "hideOverlay"));
        QVERIFY(prompt->isVisible());
        QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("nextEpisodeNow")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Up);
        m_player->togglePause(); QTRY_VERIFY(m_player->paused());
        const auto countdown = object("nextEpisodeCountdown")->property("text").toString();
        QTest::qWait(100); QCOMPARE(object("nextEpisodeCountdown")->property("text").toString(), countdown);
        m_app->seekRelative(-1); QTRY_VERIFY(!m_app->playbackPending()); QVERIFY(!prompt->isVisible());
        m_app->seekRelative(1); QTRY_VERIFY(!m_app->playbackPending()); QTRY_VERIFY(prompt->isVisible());
        m_app->stopPlayback();
    }
    void nextEpisodeStartsAutomaticallyAndNow_data() {
        QTest::addColumn<bool>("automatic");
        QTest::newRow("natural-end") << true;
        QTest::newRow("start-now") << false;
    }
    void nextEpisodeStartsAutomaticallyAndNow() {
        QFETCH(bool, automatic);
        QVERIFY(signIn()); serveEpisodeSequence();
        m_app->play(playingEpisode(95), true, {{"quality", 8000000}, {"sourceId", "source"}, {"audio", 2}, {"subtitle", -1}});
        QTRY_VERIFY(m_player->active()); QTRY_VERIFY(m_app->autoplayNextEpisode());
        m_player->loaded(); // A rendered stream reports its session before the handoff.
        if (automatic) m_player->ended();
        else {
            QVERIFY(QMetaObject::invokeMethod(object("playbackView"), "focusVideo"));
            QTest::keyClick(m_window, Qt::Key_Down); QTRY_VERIFY(object("nextEpisodeNow")->property("activeFocus").toBool());
            QTest::keyClick(m_window, Qt::Key_Return);
        }
        QTRY_COMPARE(m_app->currentItem().value("id").toString(), "episode-2");
        QTRY_VERIFY(!m_app->playbackPending()); QVERIFY(m_player->active());
        QCOMPARE(m_player->position(), 0.0); // A successor starts from the beginning, even if previously resumed.
        QCOMPARE(m_app->playbackOptions().value("quality").toInt(), 8000000);
        QCOMPARE(m_app->playbackOptions().value("sourceId").toString(), "next-source");
        QVERIFY(!m_app->playbackOptions().contains("audio")); QVERIFY(!m_app->playbackOptions().contains("subtitle"));
        QTRY_COMPARE(m_app->nextEpisode().value("id").toString(), "episode-3");
        QVERIFY(!object("nextEpisodePrompt")->property("visible").toBool());
        bool stopped = false, started = false;
        for (const auto &request : m_network->requests) {
            const auto body = QJsonDocument::fromJson(request.body).object();
            if (request.request.url().path().endsWith("Sessions/Playing/Stopped")) {
                stopped = body.value("ItemId") == "episode-1" && body.value("PlaySessionId") == "session"
                    && body.value("AudioStreamIndex").toInt() == 2 && body.value("SubtitleStreamIndex").toInt() == -1;
            }
            if (request.request.url().path().endsWith("episode-2/PlaybackInfo")) {
                started = body.value("StartTimeTicks").toInteger() == 0 && !body.contains("MediaSourceId")
                    && !body.contains("AudioStreamIndex") && !body.contains("SubtitleStreamIndex");
            }
        }
        QVERIFY(stopped); QVERIFY(started); m_app->stopPlayback();
    }
    void closingNextEpisodeCancelsUntilEpisodeEnds() {
        QVERIFY(signIn()); serveEpisodeSequence();
        const auto original = m_network->respond;
        m_network->respond = [original](const FixtureRequest &request) {
            if (request.request.url().path().contains("MediaSegments/"))
                return FixtureResponse{R"({"Items":[{"Type":"Outro","StartTicks":600000000,"EndTicks":1200000000}]})"};
            return original(request);
        };
        m_app->play(playingEpisode(95), true, {{"quality", 8000000}});
        QTRY_VERIFY(m_player->active()); QTRY_VERIFY(m_app->autoplayNextEpisode());
        QVERIFY(QMetaObject::invokeMethod(object("playbackView"), "focusVideo"));
        QTest::keyClick(m_window, Qt::Key_Down); QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_VERIFY(object("nextEpisodeClose")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return);
        QVERIFY(!m_app->autoplayNextEpisode()); QVERIFY(m_player->active());
        QVERIFY(!object("nextEpisodePrompt")->property("visible").toBool());
        QVERIFY(!object("skipSegmentButton")->property("visible").toBool());
        m_app->seekRelative(-50); QTRY_VERIFY(!m_app->playbackPending());
        m_app->seekRelative(50); QTRY_VERIFY(!m_app->playbackPending());
        m_app->setQuality(4000000); QTRY_VERIFY(!m_app->playbackPending());
        QVERIFY(!m_app->autoplayNextEpisode()); QVERIFY(!object("nextEpisodePrompt")->property("visible").toBool());
        QVERIFY(!object("skipSegmentButton")->property("visible").toBool());
        m_player->ended(); QVERIFY(!m_player->active()); QVERIFY(m_app->nextEpisode().isEmpty());
        QVERIFY(!object("playbackView")->property("visible").toBool());
        QTRY_VERIFY(!m_app->busy());
        m_app->play(playingEpisode(95), true, {{"quality", 8000000}});
        QTRY_VERIFY(m_player->active()); QTRY_VERIFY(m_app->autoplayNextEpisode());
        QVERIFY(object("nextEpisodePrompt")->property("visible").toBool());
        m_app->stopPlayback();
    }
    void unavailableAndStaleNextEpisodes_data() {
        QTest::addColumn<QString>("action");
        QTest::newRow("lookup-error") << "lookup-error";
        QTest::newRow("last-episode") << "last-episode";
        QTest::newRow("stop-before-reply") << "stop";
        QTest::newRow("end-before-reply") << "end";
        QTest::newRow("different-title-before-reply") << "different-title";
        QTest::newRow("playback-error") << "playback-error";
    }
    void unavailableAndStaleNextEpisodes() {
        QFETCH(QString, action);
        QVERIFY(signIn()); serveEpisodeSequence(100, action == "lookup-error" ? 404 : 200);
        auto item = playingEpisode(95);
        if (action == "last-episode") item["id"] = "episode-3";
        m_app->play(item, true, {{"quality", 8000000}}); QTRY_VERIFY(m_player->active());
        if (action == "stop") m_app->stopPlayback();
        else if (action == "end") m_player->ended();
        else if (action == "different-title") {
            m_app->play({{"id", "movie"}, {"type", "Movie"}, {"runtime", 120}}, false);
            QTRY_VERIFY(!m_app->playbackPending());
        } else if (action == "playback-error") {
            QTRY_VERIFY(m_app->autoplayNextEpisode());
            m_player->error("Playback failed");
            QCOMPARE(m_app->message(), "Playback failed"); QVERIFY(!m_player->active());
        }
        QTest::qWait(150);
        QVERIFY(m_app->nextEpisode().isEmpty()); QVERIFY(!m_app->autoplayNextEpisode());
        QVERIFY(!object("nextEpisodePrompt")->property("visible").toBool());
        QVERIFY(std::none_of(m_network->requests.begin(), m_network->requests.end(), [](const auto &request) {
            return request.request.url().path().endsWith("episode-2/PlaybackInfo");
        }));
        if (action == "lookup-error" || action == "last-episode") {
            QVERIFY(m_app->message().isEmpty()); m_player->ended(); QVERIFY(!m_player->active());
        }
        m_app->stopPlayback();
    }
    void overlayBackUpAndPauseScreen() {
        QVERIFY(signIn());
        auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        item["type"] = "Episode"; item["seriesName"] = "A series"; item["seasonNumber"] = 2; item["episodeNumber"] = 4;
        m_window->resize(800, 500);
        m_app->play(item); QTRY_VERIFY(m_player->active());
        QCOMPARE(object("playerTitle")->property("text").toString(), "A series");
        QVERIFY(object("playerEpisode")->property("text").toString().contains("S2 · E4"));
        auto *view = qobject_cast<QQuickItem *>(object("playbackView")); QVERIFY(view); view->forceActiveFocus();
        QVERIFY(object("playerOverlay")->property("visible").toBool());
        QVERIFY(!object("endingTime")->property("text").toString().isEmpty());
        const auto controlsPreview = qEnvironmentVariable("JF_PLAYER_SCREENSHOT");
        if (!controlsPreview.isEmpty()) QVERIFY(m_window->grabWindow().save(controlsPreview));
        qobject_cast<QQuickItem *>(object("playerSettings"))->forceActiveFocus();
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("playerTrackPopup")->property("visible").toBool());
        QTRY_VERIFY(object("playingQuality")->property("activeFocus").toBool());
        qobject_cast<QQuickItem *>(object("playerInfo"))->forceActiveFocus();
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(object("mediaInfoPopup")->property("visible").toBool());
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!object("mediaInfoPopup")->property("visible").toBool());
        QVERIFY(object("playerTrackPopup")->property("visible").toBool());
        QTRY_VERIFY(object("playingQuality")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QTest::keyClick(m_window, Qt::Key_End); QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_COMPARE(m_app->playbackOptions().value("quality").toInt(), 4000000);
        QTRY_VERIFY(!m_app->playbackPending()); QCOMPARE(m_app->playMethod(), "Transcode");
        QVERIFY(!object("playerTrackPopup")->property("visible").toBool());
        QTRY_COMPARE(m_window->activeFocusItem(), view);
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("playerSeekBar")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("playerPause")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_VERIFY(object("playerSeekBar")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_VERIFY(!object("playerOverlay")->property("visible").toBool());
        QVERIFY(m_player->active());
        QTest::keyClick(m_window, Qt::Key_Down);
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!object("playerOverlay")->property("visible").toBool());
        QVERIFY(m_player->active());
        m_player->togglePause(); QTRY_VERIFY(m_player->paused());
        auto *timer = object("pauseScreenTimer"); QVERIFY(timer); QCOMPARE(timer->property("interval").toInt(), 10000);
        timer->setProperty("interval", 120);
        QTRY_VERIFY(object("pauseScreen")->property("visible").toBool());
        const auto screenshot = qEnvironmentVariable("JF_PAUSE_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(m_window->grabWindow().save(screenshot));
        QVERIFY(!object("playerOverlay")->property("visible").toBool());
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(!object("pauseScreen")->property("visible").toBool());
        QVERIFY(m_player->paused());
        QTest::keyClick(m_window, Qt::Key_Up);
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!m_player->active());
    }
    void skeletonsTrackPendingCollectionsAndDetails() {
        QVERIFY(signIn());
        const auto original = m_network->respond;
        m_network->respond = [original](const FixtureRequest &request) {
            auto result = original(request); result.delay = 150; return result;
        };
        m_app->home();
        QVERIFY(m_app->busy());
        QVERIFY(m_app->shelves().first().toMap().value("loading").toBool());
        auto *shelf = object("shelf-0"); QVERIFY(shelf);
        QVERIFY(shelf->property("visible").toBool()); QVERIFY(shelf->property("loading").toBool());
        QTRY_VERIFY(!m_app->busy());
        QVERIFY(!m_app->shelves().first().toMap().value("loading").toBool());
        const QVariant item = m_app->shelves().first().toMap().value("items").toList().first();
        QVERIFY(QMetaObject::invokeMethod(m_window, "activate", Q_ARG(QVariant, item)));
        QVERIFY(m_app->detailLoading()); QVERIFY(object("detailLoadingSkeleton")->property("visible").toBool());
        QTRY_VERIFY(!m_app->detailLoading());
        QVERIFY(!object("detailLoadingSkeleton")->property("visible").toBool());
    }
    void restoredAuthenticationHidesLoginUntilResolved() {
        const auto original = m_network->respond;
        m_network->respond = [original](const FixtureRequest &request) {
            if (request.request.url().path().endsWith("/Users/Me"))
                return FixtureResponse{R"({"Id":"user","Name":"Fixture"})", 200, 120};
            return original(request);
        };
        m_settings->put("server", "https://fixture.invalid/base");
        m_settings->put("accessToken", "fixture-token"); m_settings->put("userId", "user");
        m_app->start(); QVERIFY(m_app->authenticating());
        QVERIFY(!object("loginScreen")->property("visible").toBool());
        QVERIFY(object("authLoadingScreen")->property("visible").toBool());
        QTRY_VERIFY(m_app->signedIn() && !m_app->authenticating());
        QVERIFY(!object("loginScreen")->property("visible").toBool());
        QVERIFY(!object("authLoadingScreen")->property("visible").toBool());
        m_app->logout();
        m_network->respond = [original](const FixtureRequest &request) {
            if (request.request.url().path().endsWith("/Users/Me")) return FixtureResponse{"{}", 401, 80};
            return original(request);
        };
        m_settings->put("accessToken", "expired"); m_settings->put("userId", "user");
        m_app->start(); QVERIFY(m_app->authenticating());
        QTRY_VERIFY(!m_app->authenticating());
        QVERIFY(!m_app->signedIn()); QVERIFY(object("loginScreen")->property("visible").toBool());
    }
    void firstHorizontalInputUsesSeekPreview_data() {
        QTest::addColumn<int>("axis");
        QTest::addColumn<int>("key");
        QTest::addColumn<bool>("hidden");
        QTest::newRow("small-stick-right") << 12000 << 0 << false;
        QTest::newRow("full-stick-right") << 30000 << 0 << false;
        QTest::newRow("small-stick-left-hidden") << -12000 << 0 << true;
        QTest::newRow("full-stick-left-hidden") << -30000 << 0 << true;
        QTest::newRow("right-key") << 0 << int(Qt::Key_Right) << false;
        QTest::newRow("left-key-hidden") << 0 << int(Qt::Key_Left) << true;
    }
    void firstHorizontalInputUsesSeekPreview() {
        QFETCH(int, axis); QFETCH(int, key); QFETCH(bool, hidden);
        QVERIFY(signIn());
        const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        QVERIFY(index >= 0);
        auto *stick = SDL_JoystickOpen(index); QVERIFY(stick);
        const auto releaseStick = qScopeGuard([&] { SDL_JoystickClose(stick); SDL_JoystickDetachVirtual(index); });
        QTRY_VERIFY(m_input->name() != "Keyboard");
        const auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        m_app->play(item, true, {{"quality", 8000000}});
        QTRY_VERIFY(m_player->active());
        auto *view = qobject_cast<QQuickItem *>(object("playbackView")); QVERIFY(view);
        auto *bar = object("playerSeekBar"); QVERIFY(bar);
        QTRY_COMPARE(m_window->activeFocusItem(), view);
        if (hidden) {
            QTest::keyClick(m_window, Qt::Key_Up);
            QVERIFY(!object("playerOverlay")->property("visible").toBool());
        }
        const double position = m_player->position();
        QVERIFY(position > 0);
        const auto requestsBefore = m_network->requests.size();
        if (axis) QCOMPARE(SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, axis), 0);
        else QTest::keyClick(m_window, Qt::Key(key));
        QTRY_VERIFY(bar->property("activeFocus").toBool());
        QTRY_VERIFY(bar->property("seeking").toBool());
        const bool backwards = axis < 0 || key == Qt::Key_Left;
        QTRY_VERIFY(backwards ? bar->property("previewPosition").toDouble() < position
                             : bar->property("previewPosition").toDouble() > position);
        QTest::qWait(450); // Holding the first movement must not trigger the old repeated seek path.
        QCOMPARE(SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, 0), 0);
        QTRY_COMPARE(m_input->horizontalAxis(), 0.0);
        QVERIFY(object("playerOverlay")->property("visible").toBool());
        QCOMPARE(m_player->position(), position);
        QCOMPARE(m_network->requests.size(), requestsBefore);
        QTest::keyClick(m_window, Qt::Key_Escape);
        QVERIFY(!bar->property("seeking").toBool());
        QCOMPARE(m_player->position(), position);
        m_app->stopPlayback();
    }
    void analogTimelinePreviewsAcceleratesAndCommitsOnce() {
        QVERIFY(signIn()); m_window->resize(1920, 1080);
        const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        QVERIFY(index >= 0); auto *stick = SDL_JoystickOpen(index); QVERIFY(stick);
        QTRY_VERIFY(m_input->name() != "Keyboard");
        auto item = m_app->shelves().first().toMap()["items"].toList().first().toMap();
        item["trickplay"] = QVariantMap{{"source", QVariantMap{{"320", QVariantMap{{"Width", 320}, {"Height", 180}, {"TileWidth", 10}, {"TileHeight", 10}, {"ThumbnailCount", 720}, {"Interval", 10000}}}}}};
        m_app->play(item, false, {{"quality", 8000000}}); QTRY_VERIFY(m_player->active());
        auto *view = qobject_cast<QQuickItem *>(object("playbackView")); view->forceActiveFocus();
        QTest::keyClick(m_window, Qt::Key_Down);
        auto *bar = object("playerSeekBar"); QVERIFY(bar);
        QTRY_VERIFY(bar->property("activeFocus").toBool()); QVERIFY(m_input->timelineMode());
        const auto requestsBefore = m_network->requests.size();
        SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, 12000);
        QTRY_VERIFY(bar->property("previewPosition").toDouble() > 1);
        const auto slowSpeed = bar->property("speed").toDouble();
        SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, 30000);
        QTRY_VERIFY(bar->property("speed").toDouble() > slowSpeed * 3);
        const auto fastSpeed = bar->property("speed").toDouble();
        QTest::qWait(350); QVERIFY(bar->property("speed").toDouble() > fastSpeed);
        SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, 0); QTest::qWait(60);
        const auto target = bar->property("previewPosition").toDouble(); QTest::qWait(100);
        QCOMPARE(bar->property("previewPosition").toDouble(), target);
        QCOMPARE(m_network->requests.size(), requestsBefore); // No stream restarts while previewing.
        const auto frame = m_app->trickplayFrame(1015);
        QCOMPARE(frame.value("row").toInt(), 0); QCOMPARE(frame.value("column").toInt(), 1);
        QVERIFY(QUrl(frame.value("url").toString()).path().endsWith("/Trickplay/320/1.jpg"));
        QTest::keyClick(m_window, Qt::Key_Return); QTRY_VERIFY(!m_app->playbackPending());
        QVERIFY(qAbs(m_player->position() - target) < 0.001);
        QVERIFY(!bar->property("seeking").toBool());
        QTest::keyClick(m_window, Qt::Key_Left); QVERIFY(bar->property("seeking").toBool());
        QTest::keyClick(m_window, Qt::Key_Escape); QVERIFY(!bar->property("seeking").toBool());
        QVERIFY(qAbs(m_player->position() - target) < 0.001);
        QTest::keyClick(m_window, Qt::Key_Down); QVERIFY(!m_input->timelineMode());
        SDL_JoystickClose(stick); SDL_JoystickDetachVirtual(index);
        m_app->stopPlayback();
    }
    void embeddedMpvDecodesPausesAndResumes() {
        m_engine.reset();
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty()) QSKIP("ffmpeg is required to generate the playback fixture.");
        QTemporaryDir dir; const QString clip = dir.filePath("clip.mp4");
        QProcess encoder;
        encoder.start(ffmpeg, {"-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10",
            "-f", "lavfi", "-i", "sine=frequency=400", "-f", "lavfi", "-i", "sine=frequency=800",
            "-map", "0:v", "-map", "1:a", "-map", "2:a", "-t", "3", "-c:v", "mpeg4", "-c:a", "aac", "-y", clip});
        QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0);
        mpv_render_context *raw = nullptr;
        mpv_render_param init[] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        QCOMPARE(mpv_render_context_create(&raw, m_player->core()->handle, init), 0);
        std::unique_ptr<mpv_render_context, decltype(&mpv_render_context_free)> renderer(raw, mpv_render_context_free);
        QByteArray pixels(64 * 64 * 4, 0); int size[] = {64, 64}; size_t stride = 64 * 4; int frames = 0;
        QTimer renderTimer;
        connect(&renderTimer, &QTimer::timeout, this, [&] {
            if (!(mpv_render_context_update(raw) & MPV_RENDER_UPDATE_FRAME)) return;
            mpv_render_param params[] = {{MPV_RENDER_PARAM_SW_SIZE, size}, {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
                {MPV_RENDER_PARAM_SW_STRIDE, &stride}, {MPV_RENDER_PARAM_SW_POINTER, pixels.data()}, {MPV_RENDER_PARAM_INVALID, nullptr}};
            mpv_render_context_render(raw, params); ++frames;
        });
        mpv_set_property_string(m_player->core()->handle, "options/ao", "null");
        renderTimer.start(15); m_player->rendererReady();
        QSignalSpy loaded(m_player.get(), &MpvPlayer::loaded);
        QSignalSpy ended(m_player.get(), &MpvPlayer::ended);
        QSignalSpy errors(m_player.get(), &MpvPlayer::error);
        m_player->load(QUrl::fromLocalFile(clip), 0, 0, 3, false);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(frames > 2 && m_player->position() > 0.2, 5000);
        QVERIFY(m_player->hasFrame());
        QVERIFY(std::any_of(pixels.begin(), pixels.end(), [](char p) { return static_cast<unsigned char>(p) > 50; }));
        m_player->togglePause(); QTRY_VERIFY(m_player->paused());
        m_player->selectStream(2, false);
        auto selectedAudio = [this] { int64_t id = 0; mpv_get_property(m_player->core()->handle, "aid", MPV_FORMAT_INT64, &id); return id; };
        QTRY_COMPARE(selectedAudio(), 2);
        m_player->selectStream(1, false); QTRY_COMPARE(selectedAudio(), 1);
        QFile subtitle(dir.filePath("captions.srt")); QVERIFY(subtitle.open(QIODevice::WriteOnly));
        subtitle.write("1\n00:00:00,000 --> 00:00:03,000\nTest subtitle\n"); subtitle.close();
        m_player->addSubtitle(QUrl::fromLocalFile(subtitle.fileName()), true);
        QTRY_VERIFY(m_player->subtitleTrack() != "Off");
        m_player->selectStream(-1, true); QTRY_COMPARE(m_player->subtitleTrack(), "Off");
        QVERIFY(m_player->bufferedPosition() >= m_player->position());
        QTRY_VERIFY(!m_player->loading());
        m_player->seek(1.2); QVERIFY(m_player->loading());
        QTRY_VERIFY(m_player->position() >= 1.1 && !m_player->loading());
        QVERIFY(m_player->paused());
        m_player->seek(0.4); m_player->seek(1.6); m_player->seek(0.8);
        QTRY_VERIFY(!m_player->loading() && qAbs(m_player->position() - 0.8) < 0.15);
        QVERIFY(m_player->paused());
        m_player->stop(); QVERIFY(!m_player->active()); QVERIFY(!m_player->hasFrame());
        QCOMPARE(m_player->position(), 0.0); QCOMPARE(m_player->duration(), 0.0);
        m_player->load(QUrl::fromLocalFile(clip), 1.0, 0, 3, false);
        QVERIFY(!m_player->hasFrame());
        QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 5000);
        QTRY_VERIFY(m_player->position() >= 0.9 && m_player->hasFrame());
        m_player->seek(2.6);
        QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 1, 5000);
        QCOMPARE(errors.size(), 0); QVERIFY(!m_player->active());
        m_player->load(QUrl::fromLocalFile(dir.filePath("missing.mp4")), 0, 0, 3, false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QCOMPARE(ended.size(), 1); // Playback failures must never trigger automatic advancement.
        m_player->stop(); renderTimer.stop();
    }
    void bufferingShowsNetworkSpeedWithoutBlockingUi() {
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost)) QSKIP("Loopback sockets are unavailable.");
        // A seekable 60-second PCM stream delivered at twice playback speed.
        QByteArray wav; QDataStream stream(&wav, QIODevice::WriteOnly); stream.setByteOrder(QDataStream::LittleEndian);
        constexpr int dataSize = 60 * 16000;
        stream.writeRawData("RIFF", 4); stream << quint32(dataSize + 36); stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4); stream << quint32(dataSize); wav.append(QByteArray(dataSize, '\0'));
        bool stalled = false;
        int chunkSize = 1600, requests = 0;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, &server, [&, socket] {
                    const auto request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
                    socket->setProperty("sent", true); ++requests;
                    const auto range = QRegularExpression("\\r\\nRange: bytes=(\\d+)-", QRegularExpression::CaseInsensitiveOption)
                        .match(QString::fromLatin1(request));
                    int offset = range.hasMatch() ? range.captured(1).toInt() : 0;
                    const auto length = wav.size();
                    QByteArray headers = range.hasMatch() ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
                    if (range.hasMatch()) headers += "Content-Range: bytes " + QByteArray::number(offset) + "-"
                        + QByteArray::number(length - 1) + "/" + QByteArray::number(length) + "\r\n";
                    socket->write(headers + "Content-Type: audio/wav\r\nAccept-Ranges: bytes\r\nContent-Length: "
                        + QByteArray::number(length - offset) + "\r\nConnection: close\r\n\r\n");
                    auto *sender = new QTimer(socket);
                    connect(sender, &QTimer::timeout, socket, [&, socket, sender, offset]() mutable {
                        if (stalled) return;
                        const auto chunk = wav.mid(offset, chunkSize);
                        socket->write(chunk); offset += chunk.size();
                        if (offset >= wav.size()) { sender->stop(); socket->disconnectFromHost(); }
                    });
                    sender->start(50);
                });
            }
        });
        // Null output decodes the stream while Qt Quick uses its headless backend.
        QCOMPARE(mpv_set_property_string(m_player->core()->handle, "options/vo", "null"), 0);
        QCOMPARE(mpv_set_property_string(m_player->core()->handle, "options/ao", "null"), 0);
        m_player->rendererReady();
        m_player->load(QUrl(QString("http://127.0.0.1:%1/clip.wav").arg(server.serverPort())), 0, 0, 60, false);
        QVERIFY(m_player->loading());
        QVERIFY(object("videoBlank")->property("visible").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(m_player->networkSpeed() > 0 && m_player->loading(), 5000);
        QVERIFY(object("bufferIndicator")->property("visible").toBool());
        QVERIFY(object("bufferStatus")->property("text").toString().contains("B/s"));
        QTRY_VERIFY_WITH_TIMEOUT(!m_player->loading(), 8000);
        QVERIFY(!object("videoBlank")->property("visible").toBool());
        m_player->setPaused(true);
        stalled = true;
        const int requestsBefore = requests;
        int ticks = 0; QTimer uiTimer;
        connect(&uiTimer, &QTimer::timeout, this, [&] { ++ticks; }); uiTimer.start(20);
        m_player->seek(45);
        QVERIFY(m_player->loading());
        QVERIFY(object("bufferIndicator")->property("visible").toBool());
        QTest::qWait(300);
        QVERIFY(ticks >= 5); QVERIFY(m_player->loading());
        // Removing the throttle lets prefetch run all the way to EOF while paused.
        stalled = false; chunkSize = 65536;
        QTRY_VERIFY_WITH_TIMEOUT(!m_player->loading() && qAbs(m_player->position() - 45) < 0.3, 5000);
        QVERIFY(requests > requestsBefore); QVERIFY(m_player->paused());
        QTRY_VERIFY_WITH_TIMEOUT(m_player->bufferedPosition() >= 59, 5000);
        QVERIFY(!object("bufferIndicator")->property("visible").toBool());
        const int cachedRequests = requests;
        m_player->seek(47);
        QVERIFY(m_player->loading());
        QTRY_VERIFY_WITH_TIMEOUT(!m_player->loading() && qAbs(m_player->position() - 47) < 0.3, 3000);
        m_player->seek(46);
        QTRY_VERIFY_WITH_TIMEOUT(!m_player->loading() && qAbs(m_player->position() - 46) < 0.3, 3000);
        QCOMPARE(requests, cachedRequests);
        m_player->stop();
        QVERIFY(!m_player->loading()); QCOMPARE(m_player->networkSpeed(), -1.0);
        QVERIFY(!m_player->hasFrame());
        stalled = true;
        m_player->load(QUrl(QString("http://127.0.0.1:%1/next.wav").arg(server.serverPort())), 0, 0, 60, false);
        QVERIFY(!m_player->hasFrame()); QVERIFY(object("videoBlank")->property("visible").toBool());
        QTest::qWait(100);
        QVERIFY(object("videoBlank")->property("visible").toBool());
        stalled = false; chunkSize = 65536;
        QTRY_VERIFY_WITH_TIMEOUT(m_player->hasFrame() && !m_player->loading(), 5000);
        QVERIFY(!object("videoBlank")->property("visible").toBool());
        m_player->stop();
    }
};

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software");
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    NativeTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "NativeTests.moc"
