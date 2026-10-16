#include "JellyfinClient.h"
#include "Playback.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QUuid>
#include <cstdio>

// Credentials come only from the caller's environment; responses and URLs are never printed.
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString server = qEnvironmentVariable("JF_SERVER"), username = qEnvironmentVariable("JF_USERNAME"), password = qEnvironmentVariable("JF_PASSWORD");
    if (server.isEmpty() || username.isEmpty() || password.isEmpty()) {
        std::fprintf(stderr, "Set JF_SERVER, JF_USERNAME and JF_PASSWORD, then run jf-live-check. Requires mpv on PATH.\n"); return 2;
    }
    JellyfinClient api(QUuid::createUuid().toString(QUuid::WithoutBraces)); api.configure(QUrl(server));
    QProcess decoder;
    QTimer deadline; deadline.setSingleShot(true); deadline.setInterval(60000);
    QString token, user, itemId;
    Playback::Plan plan;
    int stage = 0;
    bool terminating = false;
    auto fail = [&](const QString &message) {
        if (terminating) return;
        terminating = true; deadline.stop(); decoder.kill();
        std::fprintf(stderr, "FAIL: %s\n", qPrintable(message));
        if (token.isEmpty()) { app.exit(1); return; }
        const auto logout = [&] { api.post("Sessions/Logout", {}, [&](const auto &, const auto &) { app.exit(1); }); };
        QTimer::singleShot(2000, &app, [&] { app.exit(1); });
        if (plan.method == "Transcode" && !plan.sessionId.isEmpty()) {
            QUrlQuery q; q.addQueryItem("DeviceId", api.deviceId()); q.addQueryItem("PlaySessionId", plan.sessionId);
            api.remove("Videos/ActiveEncodings", q, [logout](const auto &, const auto &) { logout(); });
        } else logout();
    };
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] { fail("Timed out preparing or decoding the stream."); });
    std::function<void()> check;
    check = [&] {
        const QString mode = stage == 0 ? "direct" : "transcode";
        deadline.start();
        api.post("Items/" + itemId + "/PlaybackInfo", Playback::request(user, mode, 8000000, 0),
            [&, mode](const QJsonObject &result, const QString &error) {
            if (!error.isEmpty()) { fail(error); return; }
            plan = Playback::negotiate(result, QUrl(server), itemId, api.deviceId(), token, mode, 0);
            if (!plan.valid()) { fail(plan.error); return; }
            decoder.start("mpv", {"--no-config", "--vo=null", "--ao=null", "--hwdec=no", "--frames=30", "--length=4", "--speed=4", "--msg-level=all=no", "--", plan.url.toString(QUrl::FullyEncoded)});
        });
    };
    QObject::connect(&decoder, &QProcess::errorOccurred, &app, [&](QProcess::ProcessError) { fail("Could not start the mpv decoder."); });
    QObject::connect(&decoder, &QProcess::finished, &app, [&](int exitCode, QProcess::ExitStatus status) {
        decoder.readAllStandardOutput(); decoder.readAllStandardError();
        if (terminating) return;
        if (status != QProcess::NormalExit || exitCode != 0) { fail("mpv could not decode the negotiated stream."); return; }
        std::printf("PASS: %s negotiated and decoded a video sample\n", stage == 0 ? "direct play" : "forced transcoding");
        if (stage++ == 0) check();
        else {
            QUrlQuery q; q.addQueryItem("DeviceId", api.deviceId()); q.addQueryItem("PlaySessionId", plan.sessionId);
            api.remove("Videos/ActiveEncodings", q, [&](const auto &, const QString &error) {
                if (!error.isEmpty()) { fail(error); return; }
                api.post("Sessions/Logout", {}, [&](const auto &, const auto &) { deadline.stop(); app.quit(); });
            });
        }
    });
    QTimer::singleShot(0, &app, [&] {
        deadline.start();
        api.post("Users/AuthenticateByName", {{"Username", username}, {"Pw", password}}, [&](const QJsonObject &result, const QString &error) {
            if (!error.isEmpty()) { fail(error); return; }
            token = result["AccessToken"].toString(); user = result["User"].toObject()["Id"].toString();
            if (token.isEmpty() || user.isEmpty()) { fail("No authenticated session was returned."); return; }
            api.configure(QUrl(server), token); std::puts("PASS: authentication");
            api.get("Users/" + user + "/Views", {}, [&](const QJsonObject &views, const QString &error) {
                if (!error.isEmpty()) { fail(error); return; }
                std::printf("PASS: library browsing (%lld libraries)\n", static_cast<long long>(views["Items"].toArray().size()));
                QUrlQuery q; q.addQueryItem("Recursive", "true"); q.addQueryItem("IncludeItemTypes", "Movie,Episode"); q.addQueryItem("Limit", "1");
                api.get("Users/" + user + "/Items", q, [&](const QJsonObject &items, const QString &error) {
                    if (!error.isEmpty()) { fail(error); return; }
                    const auto list = items["Items"].toArray();
                    if (list.isEmpty()) { fail("The test account has no movies or episodes to play."); return; }
                    itemId = list.first().toObject()["Id"].toString(); check();
                });
            });
        });
    });
    return app.exec();
}
