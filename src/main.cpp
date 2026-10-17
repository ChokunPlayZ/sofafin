#include "AppController.h"
#include "ControllerInput.h"
#include "MpvVideo.h"
#include "BlurHash.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QCommandLineParser>
#include <QIcon>
#include <QImage>
#include <QTimer>
#include <cstdio>

int main(int argc, char *argv[]) {
    QGuiApplication::setApplicationName("Sofafin");
    QGuiApplication::setOrganizationName("Sofafin");
    QGuiApplication::setApplicationVersion("0.1.0");
    // libmpv's OpenGL renderer shares the native Qt scene graph context.
    if (qEnvironmentVariable("QT_QUICK_BACKEND") != "software") QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName("Sofafin");
    QGuiApplication::setDesktopFileName("io.github.sofafin");
    QGuiApplication::setWindowIcon(QIcon(":/assets/logo.svg"));
    QCommandLineParser args;
    args.setApplicationDescription("Native, controller-first Jellyfin client for Steam Deck and gamescope.");
    args.addHelpOption(); args.addVersionOption();
    args.addOption({"windowed", "Start in a window."});
    args.addOption({"fullscreen", "Start fullscreen."});
    args.addOption({"smoke-test", "Load the native interface and exit after two seconds."});
    args.addOption({"screenshot", "Save a screenshot before exiting (use with --smoke-test).", "path"});
    args.process(app);
    Settings settings;
    try {
        MpvPlayer player;
        ControllerInput input;
        AppController controller(&settings, &player);
        QQmlApplicationEngine engine;
        engine.addImageProvider("blurhash", new BlurHashProvider);
        qmlRegisterType<MpvVideo>("Sofafin", 1, 0, "MpvVideo");
        engine.rootContext()->setContextProperty("App", &controller);
        engine.rootContext()->setContextProperty("Player", &player);
        engine.rootContext()->setContextProperty("Preferences", &settings);
        engine.rootContext()->setContextProperty("Controller", &input);
        engine.rootContext()->setContextProperty("startWindowed", args.isSet("windowed"));
        engine.rootContext()->setContextProperty("startFullscreen", args.isSet("fullscreen"));
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        if (engine.rootObjects().isEmpty()) return 1;
        if (!args.isSet("smoke-test")) QTimer::singleShot(0, &controller, &AppController::start);
        else QTimer::singleShot(2000, &app, [&] {
            if (args.isSet("screenshot")) {
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
                if (!window || !window->grabWindow().save(args.value("screenshot"))) { app.exit(2); return; }
            }
            app.quit();
        });
        return app.exec();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Sofafin: %s\n", error.what());
        return 1;
    }
}
