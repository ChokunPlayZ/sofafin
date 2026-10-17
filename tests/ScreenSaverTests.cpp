#include "MpvPlayer.h"
#include "ScreenSaverInhibitor.h"
#include <QtTest>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDataStream>
#include <QFile>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryDir>

namespace {
const QString service = QStringLiteral("org.freedesktop.ScreenSaver");
const QString path = QStringLiteral("/org/freedesktop/ScreenSaver");
}

class FakeScreenSaver : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")
public:
    explicit FakeScreenSaver(const QDBusConnection &bus) : m_bus(bus) {}
    int requests = 0, releases = 0, invalidReleases = 0;
    quint32 nextCookie = 0; // Zero is a valid cookie, too.
    bool delayed = false, refuse = false;
    QString application, reason;
    QSet<quint32> active;
    void reply() {
        for (const auto &[message, cookie] : m_pending) m_bus.send(message.createReply(QVariantList{cookie}));
        m_pending.clear();
    }
public slots:
    quint32 Inhibit(const QString &app, const QString &why) {
        ++requests; application = app; reason = why;
        if (refuse) { sendErrorReply(QDBusError::Failed, "Inhibition unavailable"); return 0; }
        const auto cookie = nextCookie++;
        active.insert(cookie);
        if (delayed) {
            setDelayedReply(true);
            m_pending.append({message(), cookie});
        }
        return cookie;
    }
    void UnInhibit(quint32 cookie) {
        ++releases;
        if (!active.remove(cookie)) ++invalidReleases;
    }
private:
    QDBusConnection m_bus;
    QList<std::pair<QDBusMessage, quint32>> m_pending;
};

class ScreenSaverTests : public QObject {
    Q_OBJECT
    QDBusConnection m_bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "screensaver-test-service");
    std::unique_ptr<FakeScreenSaver> m_service;
    QTemporaryDir m_dir;
    QString m_clip;
private slots:
    void initTestCase() {
        QVERIFY(m_bus.isConnected());
        QVERIFY(m_bus.baseService() != QDBusConnection::sessionBus().baseService());
        QVERIFY(m_dir.isValid());
        m_clip = m_dir.filePath("clip.wav");
        QFile file(m_clip); QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
        constexpr quint32 bytes = 8000; // A quarter second of mono PCM at 16 kHz.
        stream.writeRawData("RIFF", 4); stream << quint32(bytes + 36); stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(16000) << quint32(32000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4); stream << bytes;
        file.write(QByteArray(bytes, '\0'));
    }
    void init() {
        m_service = std::make_unique<FakeScreenSaver>(m_bus);
        QVERIFY(m_bus.registerObject(path, m_service.get(), QDBusConnection::ExportAllSlots));
        QVERIFY(m_bus.registerService(service));
    }
    void cleanup() {
        m_service->reply();
        QTRY_VERIFY(m_service->active.isEmpty());
        QCOMPARE(m_service->invalidReleases, 0);
        m_bus.unregisterService(service);
        m_bus.unregisterObject(path);
        m_service.reset();
    }
    void playbackInhibitsThroughLoadingAndSeeking() {
        MpvPlayer player;
        QTest::qWait(30); // Drain the initial mpv property notifications.
        player.load(QUrl::fromLocalFile(m_clip), 0, 0, 120, false);
        QVERIFY(player.loading()); // No renderer yet: still waiting for the first frame.
        QTRY_COMPARE(m_service->active.size(), 1);
        QCOMPARE(m_service->application, "Sofafin");
        QCOMPARE(m_service->reason, "Playing video");
        QCOMPARE(*m_service->active.begin(), quint32(0));
        for (int i = 0; i < 20; ++i) player.changed();
        player.seek(15);
        QTest::qWait(30);
        QCOMPARE(m_service->requests, 1);
        player.setPaused(true);
        QTRY_VERIFY(m_service->active.isEmpty());
        player.setPaused(false);
        QTRY_COMPARE(m_service->active.size(), 1);
        QCOMPARE(m_service->requests, 2);
        player.stop();
        QTRY_VERIFY(m_service->active.isEmpty());
        player.load(QUrl::fromLocalFile(m_clip), 0, 0, 120, false);
        QTRY_COMPARE(m_service->active.size(), 1);
        // Destruction releases this last lock; cleanup verifies it.
    }
    void delayedReplyAfterPlaybackStops_data() {
        QTest::addColumn<QString>("action");
        for (const auto *action : {"pause", "stop", "destroy"}) QTest::newRow(action) << QString(action);
    }
    void delayedReplyAfterPlaybackStops() {
        QFETCH(QString, action);
        m_service->delayed = true;
        auto player = std::make_unique<MpvPlayer>();
        QTest::qWait(30);
        player->load(QUrl::fromLocalFile(m_clip), 0, 0, 120, false);
        QTRY_COMPARE(m_service->requests, 1);
        if (action == "pause") player->setPaused(true);
        else if (action == "stop") player->stop();
        else player.reset();
        m_service->reply();
        QTRY_VERIFY(m_service->active.isEmpty());
        QCOMPARE(m_service->releases, 1);
    }
    void resumeBeforeDelayedReplyUsesOneLock() {
        m_service->delayed = true;
        ScreenSaverInhibitor inhibitor;
        inhibitor.setInhibited(true);
        QTRY_COMPARE(m_service->requests, 1);
        inhibitor.setInhibited(false);
        inhibitor.setInhibited(true);
        m_service->reply();
        QTest::qWait(30);
        QCOMPARE(m_service->requests, 1);
        QCOMPARE(m_service->active.size(), 1);
        inhibitor.setInhibited(false);
        QTRY_VERIFY(m_service->active.isEmpty());
    }
    void missingServiceCanAppearDuringPlayback() {
        QVERIFY(m_bus.unregisterService(service));
        ScreenSaverInhibitor inhibitor;
        inhibitor.setInhibited(true);
        QTest::qWait(50);
        QCOMPARE(m_service->requests, 0);
        QVERIFY(m_bus.registerService(service));
        QTRY_COMPARE(m_service->active.size(), 1);
    }
    void restartedServiceDoesNotReceiveAnOldCookie() {
        const QString connection = "screensaver-test-restarted";
        auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connection);
        FakeScreenSaver replacement(bus);
        const auto restore = qScopeGuard([&] {
            bus.unregisterService(service); bus.unregisterObject(path);
            QDBusConnection::disconnectFromBus(connection);
            m_bus.registerService(service);
        });
        QVERIFY(bus.registerObject(path, &replacement, QDBusConnection::ExportAllSlots));
        m_service->delayed = true;
        ScreenSaverInhibitor inhibitor;
        inhibitor.setInhibited(true);
        QTRY_COMPARE(m_service->requests, 1);
        QVERIFY(m_bus.unregisterService(service));
        QVERIFY(bus.registerService(service));
        QTRY_COMPARE(replacement.active.size(), 1);
        m_service->reply();
        QTRY_VERIFY(m_service->active.isEmpty());
        QCOMPARE(replacement.active.size(), 1);
        QCOMPARE(replacement.releases, 0);
        inhibitor.setInhibited(false);
        QTRY_VERIFY(replacement.active.isEmpty());
        QCOMPARE(replacement.invalidReleases, 0);
    }
    void refusedInhibitionDoesNotInterruptPlayback() {
        m_service->refuse = true;
        MpvPlayer player;
        QSignalSpy errors(&player, &MpvPlayer::error);
        QTest::qWait(30);
        player.load(QUrl::fromLocalFile(m_clip), 0, 0, 120, false);
        QTRY_COMPARE(m_service->requests, 1);
        QTest::qWait(30);
        QVERIFY(player.active());
        QCOMPARE(errors.size(), 0);
        player.setPaused(true);
        m_service->refuse = false;
        player.setPaused(false);
        QTRY_COMPARE(m_service->active.size(), 1);
    }
    void endAndErrorReleaseInhibition() {
        MpvPlayer player;
        mpv_set_property_string(player.core()->handle, "options/ao", "null");
        QSignalSpy ended(&player, &MpvPlayer::ended);
        player.rendererReady();
        player.load(QUrl::fromLocalFile(m_clip), 0, 0, 0.25, false);
        QTRY_COMPARE(m_service->requests, 1);
        QTRY_COMPARE(ended.size(), 1);
        QTRY_VERIFY(m_service->active.isEmpty());
        MpvPlayer failure;
        QSignalSpy errors(&failure, &MpvPlayer::error);
        QTest::qWait(30);
        failure.load(QUrl::fromLocalFile(m_dir.filePath("missing.mp4")), 0, 0, 120, false);
        QTRY_COMPARE(m_service->requests, 2);
        QTRY_COMPARE(m_service->active.size(), 1);
        failure.rendererReady();
        QTRY_COMPARE(errors.size(), 1);
        QTRY_VERIFY(m_service->active.isEmpty());
        QCOMPARE(m_service->requests, 2);
        QCOMPARE(m_service->releases, 2);
    }
    void themeAudioDoesNotKeepTheScreenAwake() {
        MpvPlayer player(nullptr, true);
        mpv_set_property_string(player.core()->handle, "options/ao", "null");
        QSignalSpy loaded(&player, &MpvPlayer::loaded);
        player.load(QUrl::fromLocalFile(m_clip), 0, 0, 0, false);
        QTRY_COMPARE(loaded.size(), 1);
        QVERIFY(player.active());
        QTest::qWait(50);
        QCOMPARE(m_service->requests, 0);
        player.stop();
    }
    void applicationShutdownReleasesInhibition() {
        ScreenSaverInhibitor inhibitor;
        inhibitor.setInhibited(true);
        QTRY_COMPARE(m_service->active.size(), 1);
        QVERIFY(QMetaObject::invokeMethod(QCoreApplication::instance(), "aboutToQuit", Qt::DirectConnection));
        QTRY_VERIFY(m_service->active.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ScreenSaverTests)
#include "ScreenSaverTests.moc"
