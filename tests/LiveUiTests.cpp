#include "AppController.h"
#include "ControllerInput.h"
#include "MpvVideo.h"
#include "BlurHash.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QScreen>
#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>
#include <mpv/render.h>
#include <atomic>
#include <thread>
#include <chrono>

// Opt-in integration test. Uses an isolated config and never prints credentials/URLs.
class LiveUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir config;
    std::unique_ptr<Settings> preferences;
    std::unique_ptr<MpvPlayer> player;
    std::unique_ptr<ControllerInput> input;
    std::unique_ptr<AppController> controller;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window = nullptr;
    mpv_render_context *renderer = nullptr;
    std::thread renderThread;
    std::atomic_bool rendering = false;
    std::atomic_int frames = 0;
    QByteArray pixels = QByteArray(320 * 180 * 4, 0);
    QObject *object(const QString &name) {
        if (auto *found = window->findChild<QObject *>(name)) return found;
        std::function<QQuickItem *(QQuickItem *)> find = [&](QQuickItem *item) -> QQuickItem * {
            if (item->objectName() == name) return item;
            for (auto *child : item->childItems()) if (auto *found = find(child)) return found;
            return nullptr;
        };
        return find(window->contentItem());
    }
    void focus(const QString &name) { auto *item = qobject_cast<QQuickItem *>(object(name)); QVERIFY2(item, qPrintable(name)); item->forceActiveFocus(); }
    void capture(const QString &name) {
        QTest::qWait(250);
        const auto dir = qEnvironmentVariable("JF_SCREENSHOT_DIR");
        if (dir.isEmpty()) return;
        QDir().mkpath(dir);
        const auto image = window->grabWindow();
        QCOMPARE(image.size(), window->size());
        QVERIFY(image.save(dir + "/" + QString::number(window->height()) + "p-" + name + ".png"));
    }
    void inside(const QString &name) {
        auto *item = qobject_cast<QQuickItem *>(object(name)); QVERIFY2(item, qPrintable(name));
        QVERIFY2(QRectF(QPointF(), QSizeF(window->size())).adjusted(-1,-1,1,1).contains(item->mapRectToScene(item->boundingRect())), qPrintable(name));
    }
    void activate(const QVariantMap &item) { QVERIFY(QMetaObject::invokeMethod(window, "activate", Q_ARG(QVariant, QVariant(item)))); }
private slots:
    void initTestCase() {
        if (qEnvironmentVariableIsEmpty("JF_SERVER") || qEnvironmentVariableIsEmpty("JF_USERNAME") || qEnvironmentVariableIsEmpty("JF_PASSWORD"))
            QSKIP("Set JF_SERVER, JF_USERNAME and JF_PASSWORD to run the live UI tests.");
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        preferences = std::make_unique<Settings>(); preferences->setHardwareDecoding(false);
        player = std::make_unique<MpvPlayer>(); input = std::make_unique<ControllerInput>();
        mpv_set_property_string(player->core()->handle, "options/ao", "null");
        controller = std::make_unique<AppController>(preferences.get(), player.get());
        auto *theme = controller->findChild<MpvPlayer *>("themePlayer");
        mpv_set_property_string(theme->core()->handle, "options/ao", "null");
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->addImageProvider("blurhash", new BlurHashProvider);
        qmlRegisterType<MpvVideo>("JFGamescope", 1, 0, "MpvVideo");
        auto *ctx = engine->rootContext();
        ctx->setContextProperty("App", controller.get()); ctx->setContextProperty("Player", player.get());
        ctx->setContextProperty("Preferences", preferences.get()); ctx->setContextProperty("Controller", input.get());
        ctx->setContextProperty("startWindowed", true); ctx->setContextProperty("startFullscreen", false);
        engine->load(QUrl("qrc:/qml/Main.qml")); QVERIFY(!engine->rootObjects().isEmpty());
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().first()); QVERIFY(window);
        window->resize(QGuiApplication::primaryScreen()->size()); window->requestActivate();
        QVERIFY(window->height() >= 1080);
        QCOMPARE(window->size(), QGuiApplication::primaryScreen()->size());
        mpv_render_param init[] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        QCOMPARE(mpv_render_context_create(&renderer, player->core()->handle, init), 0);
        // libmpv rendering must run independently of synchronous client API
        // calls, including setPaused/track queries during stream replacement.
        rendering = true;
        renderThread = std::thread([this] {
            while (rendering) {
                if (mpv_render_context_update(renderer) & MPV_RENDER_UPDATE_FRAME) {
                    int size[] = {320, 180}, block = 0; size_t stride = 320 * 4;
                    mpv_render_param params[] = {{MPV_RENDER_PARAM_SW_SIZE, size}, {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
                        {MPV_RENDER_PARAM_SW_STRIDE, &stride}, {MPV_RENDER_PARAM_SW_POINTER, pixels.data()},
                        {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block}, {MPV_RENDER_PARAM_INVALID, nullptr}};
                    mpv_render_context_render(renderer, params); ++frames;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        });
        player->rendererReady();
    }
    void usability() {
        capture("login");
        object("serverField")->setProperty("text", qEnvironmentVariable("JF_SERVER"));
        object("usernameField")->setProperty("text", qEnvironmentVariable("JF_USERNAME"));
        object("passwordField")->setProperty("text", qEnvironmentVariable("JF_PASSWORD"));
        focus("signInButton"); QTest::keyClick(window, Qt::Key_Return);
        QVERIFY(controller->authenticating());
        QVERIFY(!object("loginScreen")->property("visible").toBool());
        capture("connecting");
        QTRY_VERIFY_WITH_TIMEOUT(controller->signedIn() && !controller->busy(), 30000);
        QVERIFY2(controller->message().isEmpty(), qPrintable(controller->message()));
        QVERIFY(controller->libraries().size() >= 1);
        qInfo("PASS: authentication and home");
        capture("home"); inside("profileButton");
        QVariantMap series;
        for (const auto &s : controller->shelves()) for (const auto &v : s.toMap().value("items").toList())
            if (v.toMap().value("type") == "Series") { series = v.toMap(); break; }
        bool paginationChecked = false;
        for (const auto &lib : controller->libraries()) {
            controller->openLibrary(lib.toMap()); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 30000);
            QVERIFY2(controller->message().isEmpty(), qPrintable(controller->message()));
            auto *grid = object("libraryGrid"); QVERIFY(grid);
            QVERIFY(grid->property("items").toList().size() > 0);
            focus("libraryGrid"); QTest::keyClick(window, Qt::Key_Down); QTest::keyClick(window, Qt::Key_Right);
            capture("library-" + lib.toMap().value("collectionType").toString());
            if (!paginationChecked && controller->hasMore()) {
                const int count = grid->property("items").toList().size();
                controller->loadMore(); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 30000);
                QVERIFY(grid->property("items").toList().size() > count); paginationChecked = true;
            }
            controller->setLibraryFilter("Favorites"); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 15000);
            controller->setLibraryFilter("All"); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 15000);
            controller->setSortOrder("Rating"); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 15000);
            if (series.isEmpty()) for (const auto &v : grid->property("items").toList()) if (v.toMap().value("type") == "Series") { series = v.toMap(); break; }
        }
        qInfo("PASS: libraries, filtering, sorting, pagination");
        const auto themeTitle = qEnvironmentVariable("JF_THEME_TEST_TITLE");
        if (!themeTitle.isEmpty()) {
            controller->search(themeTitle); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 15000);
            for (const auto &v : controller->shelves().first().toMap().value("items").toList())
                if (v.toMap().value("type") == "Series") { series = v.toMap(); break; }
        }
        QVERIFY(!series.isEmpty());
        auto *theme = controller->findChild<MpvPlayer *>("themePlayer");
        QSignalSpy themeLoads(theme, &MpvPlayer::loaded);
        activate(series); QTRY_VERIFY_WITH_TIMEOUT(!controller->detailLoading() && !controller->detail().value("seasonsLoading").toBool(), 30000);
        if (!themeTitle.isEmpty()) QTRY_VERIFY_WITH_TIMEOUT(!themeLoads.isEmpty() && theme->position() > 0.1, 15000);
        const int themeLoadCount = themeLoads.size();
        capture("series"); inside("playButton"); inside("detailTitle");
        QTest::keyClick(window, Qt::Key_Return);
        auto *season = object("seasonShelf-0"); QVERIFY(season);
        QTRY_VERIFY(season->property("activeFocus").toBool());
        QTest::keyClick(window, Qt::Key_Right); const int selected = season->property("itemIndex").toInt();
        capture("episodes");
        QTest::keyClick(window, Qt::Key_Return); QTRY_VERIFY_WITH_TIMEOUT(!controller->detailLoading(), 15000);
        const auto episode = controller->detail(); QVERIFY(episode.value("playable").toBool());
        if (!themeTitle.isEmpty()) { QTest::qWait(250); QVERIFY(theme->active()); QCOMPARE(themeLoads.size(), themeLoadCount); }
        const bool favorite = episode.value("favorite").toBool();
        controller->setFavorite(episode, !favorite); QTRY_VERIFY(!controller->updatingUserData());
        QCOMPARE(controller->detail().value("favorite").toBool(), !favorite);
        controller->setFavorite(episode, favorite); QTRY_VERIFY(!controller->updatingUserData());
        capture("episode");
        focus("preplaySettingsButton"); QTest::keyClick(window, Qt::Key_Return);
        capture("tracks");
        QVERIFY(object("preplayAudio")->property("displayText").toString().contains("Automatic ·"));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->detailLoading() && !controller->detail().value("seasonsLoading").toBool(), 30000);
        QTRY_VERIFY(object("seasonShelf-0")->property("activeFocus").toBool());
        QCOMPARE(object("seasonShelf-0")->property("itemIndex").toInt(), selected);
        QTest::keyClick(window, Qt::Key_Return); QTRY_VERIFY_WITH_TIMEOUT(!controller->detailLoading(), 15000);
        qInfo("PASS: series/episode navigation, theme continuity, favorites, track labels");
        focus("playButton"); QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(player->active() && !player->loading() && frames > 2, 30000);
        QVERIFY2(controller->message().isEmpty(), qPrintable(controller->message()));
        inside("videoSurface");
        QCOMPARE(object("videoSurface")->property("width").toInt(), window->width());
        player->setPaused(true); QTest::qWait(100);
        const double initial = player->position();
        focus("playbackView"); QTest::keyClick(window, Qt::Key_Down);
        QTRY_VERIFY(object("playerSeekBar")->property("activeFocus").toBool());
        QTest::keyClick(window, Qt::Key_Right); QTest::keyClick(window, Qt::Key_Right);
        QVERIFY(object("playerSeekBar")->property("seeking").toBool());
        const int virtualIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        QVERIFY(virtualIndex >= 0); auto *stick = SDL_JoystickOpen(virtualIndex); QVERIFY(stick);
        QTRY_VERIFY(input->name() != "Keyboard");
        SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, 12000); QTest::qWait(300);
        const auto slow = object("playerSeekBar")->property("speed").toDouble();
        SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, 28000); QTest::qWait(300);
        QVERIFY(object("playerSeekBar")->property("speed").toDouble() > slow * 2);
        SDL_JoystickSetVirtualAxis(stick, SDL_CONTROLLER_AXIS_LEFTX, 0); QTest::qWait(60);
        SDL_JoystickClose(stick); SDL_JoystickDetachVirtual(virtualIndex);
        const bool trickplay = !controller->trickplayFrame(initial + 10).isEmpty();
        if (trickplay) QTRY_COMPARE_WITH_TIMEOUT(object("trickplayImage")->property("status").toInt(), 1, 15000);
        capture("seeking");
        QTest::keyClick(window, Qt::Key_Return); QTRY_VERIFY_WITH_TIMEOUT(player->position() > initial + 5, 15000);
        QVERIFY(player->paused());
        qInfo("PASS: direct decoding, trickplay, analog seeking");
        // Pointer drag commits on release.
        auto *bar = qobject_cast<QQuickItem *>(object("playerSeekBar"));
        const auto start = bar->mapToScene(QPointF(bar->width() * 0.1, bar->height() / 2)).toPoint();
        const auto end = bar->mapToScene(QPointF(bar->width() * 0.15, bar->height() / 2)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, {}, start); QTest::mouseMove(window, end); QTest::mouseRelease(window, Qt::LeftButton, {}, end);
        QTRY_VERIFY_WITH_TIMEOUT(player->position() >= player->duration() * 0.14, 15000);
        focus("playerSettings"); QTest::keyClick(window, Qt::Key_Return);
        capture("playback-settings");
        QVERIFY(!object("playingAudio")->property("displayText").toString().endsWith("Automatic"));
        QTest::keyClick(window, Qt::Key_Escape);
        qInfo("Checking subtitle off");
        controller->setPlaybackTrack(true, -1);
        qInfo("Waiting for subtitle off stream");
        QTRY_VERIFY_WITH_TIMEOUT(!controller->playbackPending() && !player->loading(), 30000);
        QCOMPARE(player->subtitleTrack(), "Off"); QVERIFY(player->paused());
        qInfo("Checking automatic subtitles");
        controller->setPlaybackTrack(true, -2);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->playbackPending() && !player->loading(), 30000);
        QTimer playbackDiagnostic;
        connect(&playbackDiagnostic, &QTimer::timeout, this, [this] {
            qInfo("Playback check: pending=%d active=%d loading=%d frames=%d position=%.1f method=%s error=%s",
                controller->playbackPending(), player->active(), player->loading(), frames.load(), player->position(), qPrintable(controller->playMethod()), qPrintable(controller->message()));
        });
        playbackDiagnostic.start(10000);
        qInfo("Checking transcoding");
        controller->setQuality(8000000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->playbackPending() && !player->loading(), 30000);
        QCOMPARE(controller->playMethod(), "Transcode");
        const double beforeSeek = player->position();
        qInfo("Checking transcoded seeking");
        controller->seekRelative(15);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->playbackPending() && !player->loading(), 30000);
        QVERIFY(player->position() >= beforeSeek + 14); QVERIFY(player->paused());
        playbackDiagnostic.stop();
        qInfo("PASS: pointer drag, subtitles, transcoding and seeking while paused");
        controller->stopPlayback();
        controller->home(); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 30000);
        for (const double scale : {0.75, 1.0, 1.5}) {
            preferences->setUiScale(scale);
            QTest::keyClick(window, Qt::Key_F1); QTRY_VERIFY(object("optionsPopup")->property("visible").toBool());
            focus("uiScaleSetting");
            capture("scale-" + QString::number(qRound(scale * 100)));
            inside("uiScaleSetting");
            QTest::keyClick(window, Qt::Key_Right);
            QCOMPARE(preferences->uiScale(), qMin(1.5, scale + 0.05));
            QTest::keyClick(window, Qt::Key_Escape);
        }
        preferences->setUiScale(1);
        controller->search(series.value("title").toString()); QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 15000);
        QVERIFY(!controller->shelves().first().toMap().value("items").toList().isEmpty());
        capture("search");
        qInfo("Live UI passed at %dx%d; decoded %d frames; trickplay %s", window->width(), window->height(), frames.load(), trickplay ? "verified" : "not supplied for this title");
    }
    void cleanupTestCase() {
        if (controller) { controller->stopPlayback(); controller->logout(); QTest::qWait(500); }
        rendering = false;
        if (renderThread.joinable()) renderThread.join();
        engine.reset();
        if (renderer) { mpv_render_context_free(renderer); renderer = nullptr; }
        controller.reset(); input.reset(); player.reset(); preferences.reset();
    }
};
int main(int argc, char **argv) {
    QTemporaryDir displayConfig;
    const int requestedHeight = qEnvironmentVariableIntValue("JF_TEST_HEIGHT");
    const int height = requestedHeight > 0 ? requestedHeight : 1080;
    const int width = height * 16 / 9;
    QFile file(displayConfig.filePath("screen.json"));
    if (!file.open(QIODevice::WriteOnly)) return 2;
    file.write(QJsonDocument(QJsonObject{{"windowFrameMargins", false}, {"screens", QJsonArray{QJsonObject{{"name", "Usability display"}, {"width", width}, {"height", height}, {"dpr", 1}}}}}).toJson()); file.close();
    qputenv("QT_QPA_PLATFORM", ("offscreen:configfile=" + file.fileName()).toUtf8());
    qputenv("QT_QUICK_BACKEND", "software");
    // QML image errors can contain authenticated URLs; keep them out of test logs.
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
        QString safe = message;
        safe.replace(QRegularExpression("(api_key=|ApiKey=|Token=)[^&\\s\\\"]+"), "\\1[redacted]");
        std::fprintf(stderr, "%s\n", qPrintable(safe));
        if (type == QtFatalMsg) std::abort();
    });
    QQuickStyle::setStyle("Basic"); QGuiApplication app(argc, argv);
    LiveUiTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "LiveUiTests.moc"
