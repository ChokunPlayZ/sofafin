#include "AppController.h"
#include "FakeNetwork.h"
#include <QtTest>
#include <QDataStream>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

// Deliberately withhold the current episode's tail while allowing playback.
class EpisodeServer : public QTcpServer {
public:
    bool releaseCurrent = false, nextSent = false;
    QHash<QString, int> requests;
    QHash<QString, QByteArray> nextResources;
    QSet<QString> completedResources;
    EpisodeServer() {
        QDataStream stream(&wav, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        constexpr int dataSize = 60 * 16000;
        stream.writeRawData("RIFF", 4); stream << quint32(dataSize + 36); stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4); stream << quint32(dataSize); wav.append(QByteArray(dataSize, '\0'));
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    const auto request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
                    socket->setProperty("sent", true);
                    const QString path = QUrl(QString::fromLatin1(request.split(' ').value(1))).path();
                    const bool next = path.contains("episode-2");
                    const auto resource = path.section('/', -1);
                    const auto data = next && !nextResources.isEmpty() ? nextResources.value(resource) : wav;
                    ++requests[next ? "next" : "current"];
                    const auto range = QRegularExpression("\\r\\nRange: bytes=(\\d+)-", QRegularExpression::CaseInsensitiveOption)
                        .match(QString::fromLatin1(request));
                    int offset = range.hasMatch() ? range.captured(1).toInt() : 0;
                    QByteArray headers = range.hasMatch() ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
                    if (range.hasMatch()) headers += "Content-Range: bytes " + QByteArray::number(offset) + "-"
                        + QByteArray::number(data.size() - 1) + "/" + QByteArray::number(data.size()) + "\r\n";
                    const auto type = data.startsWith("#EXTM3U") ? "application/vnd.apple.mpegurl" : "application/octet-stream";
                    socket->write(headers + "Content-Type: " + type + "\r\nAccept-Ranges: bytes\r\nContent-Length: "
                        + QByteArray::number(data.size() - offset) + "\r\nConnection: close\r\n\r\n");
                    auto *sender = new QTimer(socket);
                    connect(sender, &QTimer::timeout, socket, [this, socket, sender, next, resource, data, offset]() mutable {
                        const auto limit = next || releaseCurrent ? data.size() : qsizetype(160044);
                        if (offset >= limit) return;
                        const auto chunk = data.mid(offset, qMin(qsizetype(65536), limit - offset));
                        socket->write(chunk); offset += chunk.size();
                        if (offset == data.size()) {
                            if (next) {
                                completedResources.insert(resource);
                                nextSent = nextResources.isEmpty() || completedResources.size() == nextResources.size();
                            }
                            sender->stop(); socket->disconnectFromHost();
                        }
                    });
                    sender->start(10);
                });
            }
        });
    }
private:
    QByteArray wav;
};

class EpisodeBufferTests : public QObject {
    Q_OBJECT
    QTemporaryDir config;
    std::unique_ptr<EpisodeServer> server;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<FakeNetwork> network;
    std::unique_ptr<MpvPlayer> player;
    std::unique_ptr<AppController> controller;
    int nextDelay = 0, nextStatus = 200, nextNegotiations = 0;
    bool nextDirect = true;
    QJsonObject episode(const QString &id) const {
        return {{"Id", id}, {"Name", id}, {"Type", "Episode"}, {"SeriesId", "series"}, {"RunTimeTicks", 600000000}};
    }
    QVariantMap current() const {
        return {{"id", "episode-1"}, {"title", "First"}, {"type", "Episode"}, {"seriesId", "series"}, {"runtime", 60}};
    }
    int reports(const QString &event, const QString &id) const {
        return std::count_if(network->requests.begin(), network->requests.end(), [&](const FixtureRequest &request) {
            return request.request.url().path().endsWith("Sessions/" + event)
                && QJsonDocument::fromJson(request.body).object().value("ItemId") == id;
        });
    }
    bool encodingStopped(const QString &session) const {
        return std::any_of(network->requests.begin(), network->requests.end(), [&](const FixtureRequest &request) {
            return request.operation == QNetworkAccessManager::DeleteOperation
                && QUrlQuery(request.request.url()).queryItemValue("PlaySessionId") == session;
        });
    }
private slots:
    void initTestCase() {
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    }
    void init() {
        nextDelay = 0; nextStatus = 200; nextNegotiations = 0; nextDirect = true;
        server = std::make_unique<EpisodeServer>();
        if (!server->listen(QHostAddress::LocalHost)) QSKIP("Loopback sockets are unavailable.");
        settings = std::make_unique<Settings>(); settings->clearSession(); settings->setPlaybackMode("auto");
        settings->setHardwareDecoding(false);
        network = std::make_unique<FakeNetwork>();
        network->respond = [this](const FixtureRequest &request) {
            const auto path = request.request.url().path();
            if (path.endsWith("AuthenticateByName")) return FixtureResponse{R"({"AccessToken":"token","User":{"Id":"user"}})"};
            if (path.endsWith("Shows/series/Episodes")) {
                const auto id = QUrlQuery(request.request.url()).queryItemValue("StartItemId");
                QJsonArray list{episode(id)};
                if (id == "episode-1") list.append(episode("episode-2"));
                return FixtureResponse{QJsonDocument(QJsonObject{{"Items", list}}).toJson()};
            }
            if (path.endsWith("PlaybackInfo")) {
                const auto id = path.section('/', -2, -2);
                const bool next = id == "episode-2";
                if (next) ++nextNegotiations;
                const bool direct = QJsonDocument::fromJson(request.body).object().value("EnableDirectPlay").toBool() && (!next || nextDirect);
                return FixtureResponse{QJsonDocument(QJsonObject{{"PlaySessionId", id + "-session"},
                    {"MediaSources", QJsonArray{QJsonObject{{"Id", id + "-source"}, {"SupportsDirectPlay", direct},
                        {"TranscodingUrl", "/base/Videos/" + id + "/stream"}}}}}).toJson(), next ? nextStatus : 200, next ? nextDelay : 0};
            }
            if (path.endsWith("Items/episode-1")) return FixtureResponse{QJsonDocument(episode("episode-1")).toJson()};
            if (path.endsWith("Items/episode-2")) return FixtureResponse{QJsonDocument(episode("episode-2")).toJson()};
            return FixtureResponse{};
        };
        player = std::make_unique<MpvPlayer>();
        QCOMPARE(mpv_set_property_string(player->core()->handle, "options/vo", "null"), 0);
        QCOMPARE(mpv_set_property_string(player->core()->handle, "options/ao", "null"), 0);
        player->rendererReady();
        controller = std::make_unique<AppController>(settings.get(), player.get(), nullptr, network.get());
        controller->login(QString("http://127.0.0.1:%1/base").arg(server->serverPort()), "fixture", "password", false);
        QTRY_VERIFY(controller->signedIn() && !controller->busy());
    }
    void cleanup() {
        if (controller) controller->stopPlayback();
        controller.reset(); player.reset(); network.reset(); settings.reset(); server.reset();
    }
    void waitsForFullCurrentBufferAndReusesNext_data() {
        QTest::addColumn<bool>("automatic"); QTest::addColumn<bool>("transcode");
        QTest::newRow("direct-start-now") << false << false;
        QTest::newRow("direct-autoplay") << true << false;
        QTest::newRow("transcode-start-now") << false << true;
        QTest::newRow("transcode-autoplay") << true << true;
    }
    void waitsForFullCurrentBufferAndReusesNext() {
        QFETCH(bool, automatic); QFETCH(bool, transcode);
        QTemporaryDir hls;
        if (transcode) {
            const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");
            if (ffmpeg.isEmpty()) QSKIP("ffmpeg is required to generate HLS segments.");
            QProcess encoder;
            encoder.start(ffmpeg, {"-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "sine=frequency=400",
                "-t", "12", "-c:a", "aac", "-f", "hls", "-hls_time", "2", "-hls_list_size", "0",
                "-hls_segment_filename", hls.filePath("part%d.ts"), hls.filePath("next.m3u8")});
            QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0);
            for (const auto &name : QDir(hls.path()).entryList(QDir::Files)) {
                QFile file(hls.filePath(name)); QVERIFY(file.open(QIODevice::ReadOnly));
                server->nextResources.insert(name == "next.m3u8" ? "stream" : name, file.readAll());
            }
        }
        QSignalSpy loaded(player.get(), &MpvPlayer::loaded), errors(player.get(), &MpvPlayer::error);
        controller->play(current(), false, {{"quality", transcode ? 8000000 : 0}});
        QTRY_VERIFY(player->hasFrame() && !player->loading() && controller->autoplayNextEpisode());
        player->setPaused(true);
        QTest::qWait(150);
        QVERIFY(!player->fullyBuffered()); QCOMPARE(nextNegotiations, 0); QCOMPARE(server->requests.value("next"), 0);
        server->releaseCurrent = true;
        QTRY_VERIFY(player->fullyBuffered());
        QTRY_VERIFY(server->nextSent);
        QCOMPARE(nextNegotiations, 1); QCOMPARE(loaded.size(), 1);
        QCOMPARE(controller->currentItem().value("id").toString(), "episode-1");
        QCOMPARE(reports("Playing", "episode-2"), 0);
        QTest::qWait(150); // Let the prefetched demuxer consume the final packets.
        const int nextRequests = server->requests.value("next");
        server->close(); // The handoff must succeed without reopening the media URL.
        if (automatic) { player->seek(59.7); player->setPaused(false); }
        else controller->playNextEpisode();
        QTRY_COMPARE(controller->currentItem().value("id").toString(), "episode-2");
        QTRY_COMPARE(loaded.size(), 2);
        QTRY_VERIFY(player->hasFrame() && !player->loading() && player->position() < 5);
        QCOMPARE(server->requests.value("next"), nextRequests); QCOMPARE(nextNegotiations, 1);
        QCOMPARE(reports("Playing/Stopped", "episode-1"), 1); QCOMPARE(reports("Playing", "episode-2"), 1);
        QVERIFY(!controller->playbackOptions().contains("audio")); QVERIFY(!controller->playbackOptions().contains("subtitle"));
        QCOMPARE(errors.size(), 0);
    }
    void cacheAtMemoryLimitDoesNotPrebuffer() {
        QCOMPARE(mpv_set_property_string(player->core()->handle, "demuxer-max-bytes", "64KiB"), 0);
        server->releaseCurrent = true;
        controller->play(current(), false);
        QTRY_VERIFY(player->hasFrame()); player->setPaused(true);
        const auto cacheIdle = [this] {
            int idle = 0;
            mpv_get_property(player->core()->handle, "demuxer-cache-idle", MPV_FORMAT_FLAG, &idle);
            return bool(idle);
        };
        QTRY_VERIFY(cacheIdle()); QTest::qWait(150);
        QVERIFY(!player->fullyBuffered()); QCOMPARE(nextNegotiations, 0); QCOMPARE(server->requests.value("next"), 0);
    }
    void unusedPrebufferIsCleanedUp_data() {
        QTest::addColumn<QString>("action"); QTest::addColumn<int>("delay");
        for (const auto &action : {"cancel", "stop", "quality", "different-title", "error"}) {
            QTest::newRow(action) << QString(action) << 0;
            QTest::newRow(qPrintable(QString(action) + "-pending")) << QString(action) << 250;
        }
    }
    void unusedPrebufferIsCleanedUp() {
        QFETCH(QString, action); QFETCH(int, delay);
        nextDelay = delay; nextDirect = false;
        controller->play(current(), false);
        QTRY_VERIFY(player->hasFrame()); player->setPaused(true);
        server->releaseCurrent = true;
        QTRY_COMPARE(nextNegotiations, 1);
        if (!delay) QTRY_VERIFY(server->nextSent);
        if (action == "cancel") controller->cancelNextEpisode();
        else if (action == "stop") controller->stopPlayback();
        else if (action == "quality") controller->setQuality(4000000);
        else if (action == "error") player->error("Fixture failure");
        else controller->play({{"id", "movie"}, {"runtime", 60}}, false);
        QTRY_VERIFY(encodingStopped("episode-2-session"));
        if (action == "cancel") {
            QVERIFY(player->active()); QVERIFY(!controller->autoplayNextEpisode());
            player->ended(); QVERIFY(!player->active());
        } else if (action == "stop" || action == "error") {
            QVERIFY(!player->active()); QVERIFY(controller->nextEpisode().isEmpty());
        }
        QVERIFY(controller->currentItem().value("id").toString() != "episode-2");
        QCOMPARE(reports("Playing", "episode-2"), 0);
    }
    void failedSpeculationFallsBackWithoutRetryStorm() {
        nextStatus = 500;
        controller->play(current(), false);
        QTRY_VERIFY(player->hasFrame()); player->setPaused(true);
        server->releaseCurrent = true;
        QTRY_COMPARE(nextNegotiations, 1); QTest::qWait(250);
        QVERIFY(controller->message().isEmpty()); QVERIFY(!controller->playbackPending());
        QCOMPARE(nextNegotiations, 1); QCOMPARE(server->requests.value("next"), 0);
        nextStatus = 200;
        controller->playNextEpisode();
        QTRY_COMPARE(controller->currentItem().value("id").toString(), "episode-2");
        QTRY_VERIFY(player->hasFrame() && !player->loading());
        QCOMPARE(nextNegotiations, 2);
    }
};

QTEST_GUILESS_MAIN(EpisodeBufferTests)
#include "EpisodeBufferTests.moc"
