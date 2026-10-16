#pragma once
#include "JellyfinClient.h"
#include "Settings.h"
#include "MpvPlayer.h"
#include "Playback.h"
#include <QVariantList>
#include <QTimer>

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString username READ username NOTIFY changed)
    Q_PROPERTY(QString savedServer READ savedServer CONSTANT)
    Q_PROPERTY(QString savedUsername READ savedUsername CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QVariantList shelves READ shelves NOTIFY shelvesChanged)
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY librariesChanged)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY changed)
    Q_PROPERTY(QString playMethod READ playMethod NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY changed)
public:
    AppController(Settings *settings, MpvPlayer *player, QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr);
    bool signedIn() const { return !m_userId.isEmpty(); }
    bool busy() const { return m_pending > 0 || m_negotiating; }
    QString username() const { return m_username; }
    QString savedServer() const { return m_settings->value("server").toString(); }
    QString savedUsername() const { return m_settings->value("username").toString(); }
    QString title() const { return m_title; }
    QVariantList shelves() const { return m_shelves; }
    QVariantList libraries() const { return m_libraries; }
    QString message() const { return m_message; }
    QString currentTitle() const { return m_currentItem.value("title").toString(); }
    QString playMethod() const { return m_plan.method; }
    bool hasMore() const { return m_loaded < m_total; }
    bool canGoBack() const { return !m_history.isEmpty(); }
    void start();
    Q_INVOKABLE void login(const QString &server, const QString &username, const QString &password, bool remember);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void home(const QString &category = "Home");
    Q_INVOKABLE void open(const QVariantMap &item);
    Q_INVOKABLE void back();
    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void play(const QVariantMap &item, bool resume = true);
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void seekRelative(double seconds);
    Q_INVOKABLE void clearMessage() { m_message.clear(); emit changed(); }
    Q_INVOKABLE void notify(const QString &message) { m_message = message; emit changed(); }
signals:
    void changed();
    void shelvesChanged();
    void librariesChanged();
    void browsingReady();
private:
    QVariantMap normalize(const QJsonObject &item) const;
    QVariantList items(const QJsonObject &response) const;
    void fetchPage(bool append = false);
    void beginPlay(const QVariantMap &item, qint64 startTicks);
    void report(const QString &event);
    void finishSession();
    void setError(const QString &error);
    Settings *m_settings;
    MpvPlayer *m_player;
    JellyfinClient m_api;
    QString m_userId, m_username, m_title = "Home", m_message;
    QVariantList m_shelves, m_libraries;
    QString m_pageParent, m_pageSearch, m_pageCategory;
    QList<QVariantMap> m_history;
    int m_generation = 0, m_pending = 0, m_loaded = 0, m_total = 0, m_playGeneration = 0;
    bool m_negotiating = false, m_reportedStart = false;
    QVariantMap m_currentItem;
    Playback::Plan m_plan;
    QTimer m_progress;
};
