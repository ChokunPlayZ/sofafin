#include "Playback.h"
#include "JellyfinClient.h"
#include "Settings.h"
#include "FakeNetwork.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QNetworkProxy>
#include <QTemporaryDir>

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void settingsMigration_data() {
        QTest::addColumn<bool>("legacyExists");
        QTest::addColumn<bool>("currentExists");
        QTest::newRow("fresh-install") << false << false;
        QTest::newRow("migrate-previous-install") << true << false;
        QTest::newRow("preserve-sofafin-settings") << true << true;
    }
    void settingsMigration() {
        QFETCH(bool, legacyExists);
        QFETCH(bool, currentExists);
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        QString legacyPath;
        {
            QSettings legacy(QSettings::IniFormat, QSettings::UserScope, "JF-Gamescope", "client");
            legacyPath = legacy.fileName();
            if (legacyExists) {
                legacy.setValue("deviceId", "legacy-device");
                legacy.setValue("accessToken", "legacy-token");
                legacy.setValue("userId", "legacy-user");
                legacy.setValue("server", "https://example.com/jellyfin");
                legacy.setValue("uiScale", 1.25);
                legacy.sync();
                QCOMPARE(legacy.status(), QSettings::NoError);
            }
        }
        if (currentExists) {
            QSettings current(QSettings::IniFormat, QSettings::UserScope, "Sofafin", "client");
            current.setValue("deviceId", "sofafin-device");
            current.setValue("uiScale", 0.9);
            current.sync();
            QCOMPARE(current.status(), QSettings::NoError);
        }
        {
            Settings settings;
            if (currentExists) {
                QCOMPARE(settings.value("deviceId").toString(), "sofafin-device");
                QCOMPARE(settings.uiScale(), 0.9);
                QVERIFY(settings.value("accessToken").toString().isEmpty());
            } else if (legacyExists) {
                QCOMPARE(settings.value("deviceId").toString(), "legacy-device");
                QCOMPARE(settings.value("accessToken").toString(), "legacy-token");
                QCOMPARE(settings.value("userId").toString(), "legacy-user");
                QCOMPARE(settings.value("server").toString(), "https://example.com/jellyfin");
                QCOMPARE(settings.uiScale(), 1.25);
                QVERIFY(!QFile::exists(legacyPath));
            } else {
                QVERIFY(!settings.value("deviceId").toString().isEmpty());
                QCOMPARE(settings.uiScale(), 1.0);
                QVERIFY(settings.value("accessToken").toString().isEmpty());
            }
            QSettings current(QSettings::IniFormat, QSettings::UserScope, "Sofafin", "client");
            QVERIFY(current.fileName().startsWith(config.path() + "/Sofafin/"));
            if (!currentExists) {
                const auto permissions = QFile::permissions(current.fileName());
                QVERIFY(permissions.testFlag(QFile::ReadOwner));
                QVERIFY(permissions.testFlag(QFile::WriteOwner));
                QVERIFY(!(permissions & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)));
            }
            settings.clearSession();
        }
        Settings restarted;
        QVERIFY(restarted.value("accessToken").toString().isEmpty());
        QVERIFY(restarted.value("userId").toString().isEmpty());
    }
    void basePaths() {
        QCOMPARE(Playback::endpoint(QUrl("https://example.com/jellyfin/"), "Users/Me").toString(), "https://example.com/jellyfin/Users/Me");
        QVERIFY(Playback::sameOrigin(QUrl("https://example.com/a"), QUrl("https://example.com:443/b")));
        QVERIFY(!Playback::sameOrigin(QUrl("https://evil.example/a"), QUrl("https://example.com/b")));
        QVERIFY(!Playback::sameOrigin(QUrl("http://example.com/a"), QUrl("https://example.com/b")));
        const QUrl server("https://example.com/jellyfin/");
        for (const auto &path : {"Videos/item/master.m3u8?x=1", "/Videos/item/master.m3u8?x=1", "/jellyfin/Videos/item/master.m3u8?x=1"}) {
            QCOMPARE(Playback::resourceUrl(server, QUrl(path)).toString(), "https://example.com/jellyfin/Videos/item/master.m3u8?x=1");
        }
    }
    void playbackPolicy() {
        auto direct = Playback::request("user", "direct", 8000000, 1200000000);
        QVERIFY(direct["EnableDirectPlay"].toBool());
        QVERIFY(!direct["EnableTranscoding"].toBool());
        QCOMPARE(direct["StartTimeTicks"].toInteger(), 1200000000LL);
        auto transcode = Playback::request("user", "transcode", 4000000, 0);
        QVERIFY(!transcode["EnableDirectPlay"].toBool());
        QVERIFY(!transcode["AllowVideoStreamCopy"].toBool());
        QVERIFY(transcode["EnableTranscoding"].toBool());
        QCOMPARE(transcode["DeviceProfile"].toObject()["MaxStreamingBitrate"].toInt(), 4000000);
    }
    void prefersDirectVersion() {
        const QJsonObject response{{"PlaySessionId", "session"}, {"MediaSources", QJsonArray{
            QJsonObject{{"Id", "a"}, {"TranscodingUrl", "/Videos/item/master.m3u8"}},
            QJsonObject{{"Id", "b"}, {"SupportsDirectPlay", true}}}}};
        auto p = Playback::negotiate(response, QUrl("https://example.com/jellyfin"), "item", "device", "token", "auto", 0);
        QVERIFY(p.valid()); QCOMPARE(p.mediaSourceId, "b"); QCOMPARE(p.method, "DirectPlay");
        QCOMPARE(p.url.path(), "/jellyfin/Videos/item/stream");
        QCOMPARE(QUrlQuery(p.url).queryItemValue("Static"), "true");
        QCOMPARE(QUrlQuery(p.url).queryItemValue("api_key"), "token");
    }
    void transcodeResumeAndToken() {
        QJsonObject response{{"PlaySessionId", "session"}, {"MediaSources", QJsonArray{QJsonObject{
            {"Id", "source"}, {"SupportsDirectPlay", true}, {"TranscodingUrl", "/jellyfin/Videos/item/master.m3u8?api_key=old&StartTimeTicks=900000000"}}}}};
        auto p = Playback::negotiate(response, QUrl("https://example.com/jellyfin"), "item", "device", "token", "transcode", 900000000);
        QVERIFY(p.valid()); QCOMPARE(p.method, "Transcode"); QCOMPARE(p.offset, 90.0);
        QCOMPARE(p.url.path(), "/jellyfin/Videos/item/master.m3u8");
        QCOMPARE(QUrlQuery(p.url).allQueryItemValues("api_key"), QStringList{"token"});
    }
    void rejectsForeignPlaybackUrl() {
        QJsonObject response{{"MediaSources", QJsonArray{QJsonObject{{"Id", "a"}, {"TranscodingUrl", "https://other.example/stream"}}}}};
        auto p = Playback::negotiate(response, QUrl("https://example.com"), "item", "device", "secret", "transcode", 0);
        QVERIFY(!p.valid()); QVERIFY(p.url.isEmpty()); QVERIFY(!p.error.contains("secret"));
    }
    void selectedVersionIsRespected() {
        const QJsonObject response{{"MediaSources", QJsonArray{
            QJsonObject{{"Id", "original"}, {"SupportsDirectPlay", true}},
            QJsonObject{{"Id", "alternate"}, {"SupportsDirectPlay", true}}}}};
        const auto selected = Playback::negotiate(response, QUrl("https://example.com"), "item", "device", "token", "auto", 0, "alternate");
        QVERIFY(selected.valid()); QCOMPARE(selected.mediaSourceId, "alternate");
        QVERIFY(!Playback::negotiate(response, QUrl("https://example.com"), "item", "device", "token", "auto", 0, "missing").valid());
    }
    void segmentValidationAndBoundaries() {
        const auto segments = Playback::segments({{"Items", QJsonArray{
            QJsonObject{{"Type", "Outro"}, {"StartTicks", 2000000000}, {"EndTicks", 2200000000LL}},
            QJsonObject{{"Type", "Intro"}, {"StartTicks", 100000000}, {"EndTicks", 300000000}},
            QJsonObject{{"Type", "Intro"}, {"StartTicks", -1}, {"EndTicks", 300000000}},
            QJsonObject{{"Type", "Recap"}, {"StartTicks", 100}, {"EndTicks", 50}},
            QJsonObject{{"Type", "Unknown"}, {"StartTicks", 100}, {"EndTicks", 500}},
            QJsonObject{{"Type", "Intro"}, {"EndTicks", 500}}}}});
        QCOMPARE(segments.size(), 2);
        QVERIFY(Playback::activeSegment(segments, 9.9).isEmpty());
        QCOMPARE(Playback::activeSegment(segments, 10).value("type").toString(), "Intro");
        QCOMPARE(Playback::activeSegment(segments, 29.9).value("end").toDouble(), 30.0);
        QVERIFY(Playback::activeSegment(segments, 30).isEmpty());
        QCOMPARE(Playback::activeSegment(segments, 200).value("type").toString(), "Outro");
        QVERIFY(Playback::segments({}).isEmpty());
    }
    void namedChaptersProvideSegmentFallback() {
        const QVariantList chapters{
            QVariantMap{{"name", "Outro"}, {"ticks", 12757090000LL}},
            QVariantMap{{"name", "Prologue"}, {"ticks", 0}},
            QVariantMap{{"name", "Opening"}, {"ticks", 570390000LL}},
            QVariantMap{{"name", "Part A"}, {"ticks", 1469990000LL}},
            QVariantMap{{"name", "Part B"}, {"ticks", 9369990000LL}},
            QVariantMap{{"name", "Intro"}, {"ticks", -1}},
            QVariantMap{{"name", "Credits"}}};
        const auto segments = Playback::chapterSegments(chapters, 1440);
        QCOMPARE(segments.size(), 2);
        QCOMPARE(segments.first().toMap()["type"].toString(), "Intro");
        QCOMPARE(segments.first().toMap()["start"].toDouble(), 57.039);
        QCOMPARE(segments.first().toMap()["end"].toDouble(), 146.999);
        QCOMPARE(segments.last().toMap()["type"].toString(), "Outro");
        QCOMPARE(segments.last().toMap()["end"].toDouble(), 1440.0);
        QVERIFY(Playback::activeSegment(segments, 57).isEmpty());
        QCOMPARE(Playback::activeSegment(segments, 60)["type"].toString(), "Intro");
        QVERIFY(Playback::activeSegment(segments, 146.999).isEmpty());
        QVERIFY(Playback::chapterSegments(chapters, 0).isEmpty());
    }
    void chapterSegmentTypes_data() {
        QTest::addColumn<QString>("name"); QTest::addColumn<QString>("type");
        QTest::newRow("intro") << "Intro" << "Intro";
        QTest::newRow("opening-credits") << "Opening Credits" << "Intro";
        QTest::newRow("anime-op") << " OP 2 " << "Intro";
        QTest::newRow("outro") << "Outro" << "Outro";
        QTest::newRow("ending") << "Ending" << "Outro";
        QTest::newRow("anime-ed") << "ED1" << "Outro";
        QTest::newRow("credits") << "End Credits" << "Outro";
        QTest::newRow("recap") << "Recap" << "Recap";
        QTest::newRow("previously") << "Previously on" << "Recap";
        QTest::newRow("preview") << "Preview" << "Preview";
        QTest::newRow("next-time") << "Next time" << "Preview";
        QTest::newRow("commercial") << "Commercial" << "Commercial";
        QTest::newRow("advertisement") << "Advertisement" << "Commercial";
        QTest::newRow("generic-chapter") << "Chapter 1" << "";
        QTest::newRow("story-title") << "Opening the door" << "";
        QTest::newRow("part-a") << "Part A" << "";
    }
    void chapterSegmentTypes() {
        QFETCH(QString, name); QFETCH(QString, type);
        const auto segments = Playback::chapterSegments({QVariantMap{{"name", name}, {"ticks", 0}},
            QVariantMap{{"name", "Story"}, {"ticks", 300000000}}}, 120);
        if (type.isEmpty()) { QVERIFY(segments.isEmpty()); return; }
        QCOMPARE(segments.size(), 1);
        QCOMPARE(segments.first().toMap()["type"].toString(), type);
        QCOMPARE(segments.first().toMap()["end"].toDouble(), 30.0);
    }
    void sourceTracksAndInfo() {
        const QJsonObject source{{"Container", "mkv"}, {"Bitrate", 8000000}, {"MediaStreams", QJsonArray{
            QJsonObject{{"Type", "Video"}, {"Index", 0}, {"Codec", "hevc"}, {"Width", 1920}, {"Height", 1080}},
            QJsonObject{{"Type", "Audio"}, {"Index", 2}, {"DisplayTitle", "English · AAC stereo"}},
            QJsonObject{{"Type", "Subtitle"}, {"Index", 5}, {"Language", "tha"}, {"Codec", "srt"}}}}};
        const auto audio = Playback::tracks(source, "Audio");
        QCOMPARE(audio.size(), 1); QCOMPARE(audio[0].toMap()["index"].toInt(), 2);
        QCOMPARE(audio[0].toMap()["label"].toString(), "English · AAC stereo");
        QCOMPARE(Playback::tracks(source, "Subtitle")[0].toMap()["index"].toInt(), 5);
        const auto info = Playback::mediaInfo(source);
        QVERIFY(info.contains("MKV · 8.0 Mbps")); QVERIFY(info.contains("1920×1080"));
        QVERIFY(info.contains("English · AAC stereo"));
        QVERIFY(!info.contains("api_key"));
    }
    void unavailableAndLive() {
        auto p = Playback::negotiate({}, QUrl("https://example.com"), "item", "device", "token", "direct", 0);
        QVERIFY(!p.valid()); QVERIFY(p.error.contains("Direct play"));
        QJsonObject response{{"MediaSources", QJsonArray{QJsonObject{{"SupportsDirectPlay", true}, {"RequiresOpening", true}}}}};
        QVERIFY(!Playback::negotiate(response, QUrl("https://example.com"), "item", "device", "token", "auto", 0).valid());
    }
    void authenticatedPostAndResponseParsing() {
        FakeNetwork transport;
        transport.respond = [](const auto &) { return FixtureResponse{R"({"PlaySessionId":"session","MediaSources":[]})"}; };
        JellyfinClient api("device", nullptr, &transport); api.configure(QUrl("https://example.com/base"), "token");
        bool completed = false;
        api.post("Items/item/PlaybackInfo", Playback::request("user", "auto", 8000000, 0), [&](const auto &result, const auto &error) {
            QVERIFY(error.isEmpty()); QCOMPARE(result["PlaySessionId"].toString(), "session"); completed = true;
        });
        QTRY_VERIFY(completed);
        QCOMPARE(transport.requests.size(), 1);
        const auto request = transport.requests.first();
        QCOMPARE(request.operation, QNetworkAccessManager::PostOperation);
        QCOMPARE(request.request.url().path(), "/base/Items/item/PlaybackInfo");
        QVERIFY(request.request.rawHeader("Authorization").contains("Token=\"token\""));
        QCOMPARE(request.request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::SameOriginRedirectPolicy));
        QVERIFY(QJsonDocument::fromJson(request.body).object()["EnableTranscoding"].toBool());
    }
    void rejectsMalformedJsonAndExpiresSession() {
        FakeNetwork transport;
        transport.respond = [](const auto &) { return FixtureResponse{"not json"}; };
        JellyfinClient api("device", nullptr, &transport); api.configure(QUrl("https://example.com"), "token");
        QString error;
        api.get("Users/Me", {}, [&](const auto &, const auto &e) { error = e; });
        QTRY_VERIFY(error.contains("invalid response"));
        transport.respond = [](const auto &) { return FixtureResponse{"{}", 401}; };
        QSignalSpy expired(&api, &JellyfinClient::unauthorized);
        api.get("Users/Me", {}, {});
        QTRY_COMPARE(expired.size(), 1);
    }
    void discardsResponsesFromOldSession() {
        FakeNetwork transport;
        JellyfinClient api("device", nullptr, &transport); api.configure(QUrl("https://example.com"), "old");
        bool called = false;
        api.get("Users/Me", {}, [&](const auto &, const auto &) { called = true; });
        api.configure(QUrl("https://example.com"), "new");
        QTest::qWait(20); QVERIFY(!called);
    }
    void realHttpTransport() {
        if (qEnvironmentVariableIsSet("CODEX_SANDBOX_NETWORK_DISABLED"))
            QSKIP("This sandbox forbids network sockets, including loopback.");
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost) && server.serverError() == QAbstractSocket::SocketAccessError)
            QSKIP("This sandbox forbids network sockets, including loopback.");
        QVERIFY(server.isListening());
        QByteArray request;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                request += socket->readAll();
                if (!request.contains("\r\n\r\n")) return;
                const QByteArray body = R"({"Id":"user","Name":"Test"})";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        JellyfinClient api("test-device"); api.configure(QUrl(QString("http://127.0.0.1:%1/base").arg(server.serverPort())), "test-token");
        bool completed = false; QJsonObject result; QString error;
        api.get("Users/Me", {}, [&](const auto &r, const auto &e) { completed = true; result = r; error = e; });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 3000);
        QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(result["Id"].toString(), "user");
        QVERIFY(request.startsWith("GET /base/Users/Me HTTP/1.1"));
        QVERIFY(request.contains("Token=\"test-token\""));
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
