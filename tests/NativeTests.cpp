#include "AppController.h"
#include "ControllerInput.h"
#include "MpvVideo.h"
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
    QJsonObject movie(const QString &id = "movie", const QString &title = "The Long Way Home") {
        return {{"Id", id}, {"Name", title}, {"Type", "Movie"}, {"ProductionYear", 2025},
            {"RunTimeTicks", 72000000000LL}, {"Overview", "A small crew crosses an unfamiliar ocean in search of a place to call home."},
            {"UserData", QJsonObject{{"PlaybackPositionTicks", 1200000000}}}};
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
        m_network = std::make_unique<FakeNetwork>();
        m_network->respond = [this](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            QJsonObject result;
            if (path.endsWith("AuthenticateByName")) result = {{"AccessToken", "fixture-token"}, {"User", QJsonObject{{"Id", "user"}, {"Name", "Fixture Viewer"}}}};
            else if (path.endsWith("/Views")) result = {{"Items", QJsonArray{QJsonObject{{"Id", "library"}, {"Name", "Movies"}, {"Type", "CollectionFolder"}, {"IsFolder", true}}}}};
            else if (path.endsWith("PlaybackInfo")) {
                const bool direct = QJsonDocument::fromJson(request.body).object()["EnableDirectPlay"].toBool();
                result = {{"PlaySessionId", "session"}, {"MediaSources", QJsonArray{QJsonObject{
                    {"Id", "source"}, {"SupportsDirectPlay", direct}, {"TranscodingUrl", "/base/Videos/movie/master.m3u8"}}}}};
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
        m_engine.reset(); m_app.reset(); m_input.reset(); m_player.reset(); m_network.reset(); m_settings.reset();
    }
    void controllerKeyboardAndSignInFocus() {
        auto *server = qobject_cast<QQuickItem *>(object("serverField")); QVERIFY(server); server->forceActiveFocus();
        QTest::keyClick(m_window, Qt::Key_Return);
        auto *keyboard = object("keyboard"); QVERIFY(keyboard); QTRY_VERIFY(keyboard->property("visible").toBool());
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(server->property("text").toString().endsWith('a'));
        QTest::keyClick(m_window, Qt::Key_Right); QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(server->property("text").toString().endsWith("ab"));
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!keyboard->property("visible").toBool());
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("usernameField")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Down); QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(object("rememberButton")->property("activeFocus").toBool());
        const bool before = object("rememberButton")->property("checked").toBool();
        QTest::keyClick(m_window, Qt::Key_Return);
        QCOMPARE(object("rememberButton")->property("checked").toBool(), !before);
    }
    void settingsWithDpad() {
        QTest::keyClick(m_window, Qt::Key_F1);
        QTRY_VERIFY(object("optionsPopup")->property("visible").toBool());
        QTRY_VERIFY(object("modeSetting")->property("activeFocus").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); QCOMPARE(m_settings->playbackMode(), "direct");
        QTest::keyClick(m_window, Qt::Key_Down); QTest::keyClick(m_window, Qt::Key_Return);
        QCOMPARE(m_settings->bitrate(), 4000000);
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
        QTRY_VERIFY(object("keyboard")->property("visible").toBool());
        SDL_JoystickSetVirtualButton(stick, SDL_CONTROLLER_BUTTON_A, 0);
        QTest::keyClick(m_window, Qt::Key_Escape);
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
    void embeddedMpvDecodesPausesAndResumes() {
        m_engine.reset();
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty()) QSKIP("ffmpeg is required to generate the playback fixture.");
        QTemporaryDir dir; const QString clip = dir.filePath("clip.mp4");
        QProcess encoder;
        encoder.start(ffmpeg, {"-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10", "-t", "3", "-c:v", "mpeg4", "-y", clip});
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
        renderTimer.start(15); m_player->rendererReady();
        QSignalSpy loaded(m_player.get(), &MpvPlayer::loaded);
        m_player->load(QUrl::fromLocalFile(clip), 0, 0, 3, false);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(frames > 2 && m_player->position() > 0.2, 5000);
        QVERIFY(std::any_of(pixels.begin(), pixels.end(), [](char p) { return static_cast<unsigned char>(p) > 50; }));
        m_player->togglePause(); QTRY_VERIFY(m_player->paused());
        m_player->seek(1.2); QTRY_VERIFY(m_player->position() >= 1.1);
        m_player->stop(); QVERIFY(!m_player->active());
        m_player->load(QUrl::fromLocalFile(clip), 1.0, 0, 3, false);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 5000);
        QTRY_VERIFY(m_player->position() >= 0.9);
        m_player->stop(); renderTimer.stop();
    }
};

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software");
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    NativeTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "NativeTests.moc"
