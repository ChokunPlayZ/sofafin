#include "MpvPlayer.h"
#include <QtTest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <mpv/render.h>
extern "C" {
#include <libavutil/hwcontext.h>
}

namespace {
int probePlayback(QCoreApplication &app, const QString &clip) {
    MpvPlayer player;
    mpv_render_context *raw = nullptr;
    mpv_render_param init[] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)}, {MPV_RENDER_PARAM_INVALID, nullptr}};
    if (mpv_render_context_create(&raw, player.core()->handle, init) < 0) return 2;
    std::unique_ptr<mpv_render_context, decltype(&mpv_render_context_free)> renderer(raw, mpv_render_context_free);
    QByteArray pixels(64 * 64 * 4, 0); int size[] = {64, 64}; size_t stride = 64 * 4;
    int frames = 0, loads = 0;
    QObject::connect(&player, &MpvPlayer::loaded, &app, [&] { ++loads; frames = 0; });
    QObject::connect(&player, &MpvPlayer::error, &app, [&] { app.exit(3); });
    QTimer renderTimer;
    QObject::connect(&renderTimer, &QTimer::timeout, &app, [&] {
        if (!(mpv_render_context_update(raw) & MPV_RENDER_UPDATE_FRAME)) return;
        mpv_render_param params[] = {{MPV_RENDER_PARAM_SW_SIZE, size}, {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
            {MPV_RENDER_PARAM_SW_STRIDE, &stride}, {MPV_RENDER_PARAM_SW_POINTER, pixels.data()}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        mpv_render_context_render(raw, params); ++frames;
        if (frames < 3 || player.position() < 0.2) return;
        if (loads == 1) {
            player.stop();
            player.load(QUrl::fromLocalFile(clip), 0.5, 0, 3, true);
        } else if (loads == 2 && player.position() >= 0.5) {
            app.exit(0);
        }
    });
    renderTimer.start(10);
    QTimer::singleShot(8000, &app, [&] { app.exit(4); });
    player.rendererReady();
    player.load(QUrl::fromLocalFile(clip), 0, 0, 3, true);
    const int result = app.exec();
    player.stop();
    return result;
}
}

class PlaybackStartupTests : public QObject {
    Q_OBJECT
private slots:
    void startsAndRestartsWithoutVulkanWhenHardwareIsUnavailable() {
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty()) QSKIP("ffmpeg is required to generate the H.264 fixture.");
        QTemporaryDir dir; QVERIFY(dir.isValid()); const auto clip = dir.filePath("startup.mp4");
        QProcess encoder;
        encoder.start(ffmpeg, {"-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10", "-t", "3", "-c:v", "libx264", "-preset", "ultrafast", "-y", clip});
        QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0);
        auto environment = QProcessEnvironment::systemEnvironment();
        const auto previousPreload = environment.value("LD_PRELOAD");
        environment.insert("LD_PRELOAD", QString::fromUtf8(JF_HWDEC_GUARD_PATH) + (previousPreload.isEmpty() ? "" : ":" + previousPreload));
        QProcess child; child.setProcessEnvironment(environment);
        // First prove the fatal guard is installed; it replaces device creation in libavutil.
        child.start(QCoreApplication::applicationFilePath(), {"--check-guard"});
        QVERIFY(child.waitForFinished(10000)); QCOMPARE(child.exitStatus(), QProcess::NormalExit); QCOMPARE(child.exitCode(), 86);
        child.start(QCoreApplication::applicationFilePath(), {"--probe", clip});
        QVERIFY(child.waitForFinished(15000));
        const auto output = child.readAllStandardError();
        QVERIFY2(child.exitStatus() == QProcess::NormalExit, output.constData());
        QVERIFY2(child.exitCode() == 0, output.constData());
        QVERIFY(!output.contains("Unexpected Vulkan"));
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1) == "--check-guard") {
        AVBufferRef *device = nullptr;
        av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
        av_buffer_unref(&device);
        return 5; // The guard must have exited before this point.
    }
    if (app.arguments().value(1) == "--probe") return probePlayback(app, app.arguments().value(2));
    PlaybackStartupTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "PlaybackStartupTests.moc"
