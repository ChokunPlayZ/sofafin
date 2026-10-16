#include "Playback.h"
#include "JellyfinClient.h"
#include "FakeNetwork.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxy>

class CoreTests : public QObject {
    Q_OBJECT
private slots:
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
