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
    Q_PROPERTY(bool authenticating READ authenticating NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool playbackPending READ playbackPending NOTIFY changed)
    Q_PROPERTY(QString username READ username NOTIFY changed)
    Q_PROPERTY(QString profileImage READ profileImage NOTIFY changed)
    Q_PROPERTY(QString savedServer READ savedServer CONSTANT)
    Q_PROPERTY(QString savedUsername READ savedUsername CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QVariantList shelves READ shelves NOTIFY shelvesChanged)
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY librariesChanged)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY changed)
    Q_PROPERTY(QString playMethod READ playMethod NOTIFY changed)
    Q_PROPERTY(QVariantMap currentItem READ currentItem NOTIFY changed)
    Q_PROPERTY(QVariantMap currentSource READ currentSource NOTIFY changed)
    Q_PROPERTY(QVariantMap playbackOptions READ playbackOptions NOTIFY changed)
    Q_PROPERTY(QVariantList mediaSegments READ mediaSegments NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY changed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailsChanged)
    Q_PROPERTY(bool detailLoading READ detailLoading NOTIFY detailsChanged)
    Q_PROPERTY(bool updatingUserData READ updatingUserData NOTIFY detailsChanged)
    Q_PROPERTY(QString libraryFilter READ libraryFilter NOTIFY changed)
    Q_PROPERTY(QString sortOrder READ sortOrder NOTIFY changed)
    Q_PROPERTY(bool canFilter READ canFilter NOTIFY changed)
public:
    AppController(Settings *settings, MpvPlayer *player, QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr);
    bool signedIn() const { return !m_userId.isEmpty(); }
    bool authenticating() const { return m_authenticating; }
    bool busy() const { return m_pending > 0 || m_negotiating; }
    bool playbackPending() const { return m_negotiating; }
    QString username() const { return m_username; }
    QString profileImage() const { return m_profileImage; }
    QString savedServer() const { return m_settings->value("server").toString(); }
    QString savedUsername() const { return m_settings->value("username").toString(); }
    QString title() const { return m_title; }
    QVariantList shelves() const { return m_shelves; }
    QVariantList libraries() const { return m_libraries; }
    QString message() const { return m_message; }
    QString currentTitle() const { return m_currentItem.value("title").toString(); }
    QString playMethod() const { return m_plan.method; }
    QVariantMap currentItem() const { return m_currentItem; }
    QVariantMap currentSource() const;
    QVariantMap playbackOptions() const { return m_playbackOptions; }
    QVariantList mediaSegments() const { return m_segments; }
    bool hasMore() const { return m_loaded < m_total; }
    bool canGoBack() const { return !m_history.isEmpty(); }
    QVariantMap detail() const { return m_detail; }
    bool detailLoading() const { return m_detailLoading; }
    bool updatingUserData() const { return m_updatingUserData; }
    QString libraryFilter() const { return m_filter; }
    QString sortOrder() const { return m_sort; }
    bool canFilter() const { return signedIn() && m_pageCategory != "Home" && m_pageType != "Series" && m_pageType != "Season"; }
    void start();
    Q_INVOKABLE void login(const QString &server, const QString &username, const QString &password, bool remember);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void home(const QString &category = "Home");
    Q_INVOKABLE void open(const QVariantMap &item);
    Q_INVOKABLE void openLibrary(const QVariantMap &item);
    Q_INVOKABLE void back();
    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void showDetails(const QVariantMap &item);
    Q_INVOKABLE void closeDetails();
    Q_INVOKABLE void openSeries(const QVariantMap &item);
    Q_INVOKABLE void openSeason(const QVariantMap &item);
    Q_INVOKABLE void setFavorite(const QVariantMap &item, bool favorite);
    Q_INVOKABLE void setPlayed(const QVariantMap &item, bool played);
    Q_INVOKABLE void setLibraryFilter(const QString &filter);
    Q_INVOKABLE void setSortOrder(const QString &sort);
    Q_INVOKABLE void play(const QVariantMap &item, bool resume = true, const QVariantMap &options = {});
    Q_INVOKABLE void setQuality(int bitrate);
    Q_INVOKABLE void setPlaybackTrack(bool subtitle, int index);
    Q_INVOKABLE void skipSegment();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void seekRelative(double seconds);
    Q_INVOKABLE QVariantMap trickplayFrame(double seconds) const;
    Q_INVOKABLE void clearMessage() { m_message.clear(); emit changed(); }
    Q_INVOKABLE void notify(const QString &message) { m_message = message; emit changed(); }
signals:
    void changed();
    void shelvesChanged();
    void librariesChanged();
    void browsingReady();
    void detailsChanged();
private:
    QVariantMap normalize(const QJsonObject &item) const;
    QVariantList items(const QJsonObject &response) const;
    void fetchPage(bool append = false);
    void beginPlay(const QVariantMap &item, qint64 startTicks, bool paused = false);
    void fetchSegments();
    void report(const QString &event);
    void finishSession();
    void setError(const QString &error);
    void updateUserData(const QString &id, const QJsonObject &data);
    void changeUserData(const QVariantMap &item, bool value, bool favorite);
    void fetchDetailSeasons(int generation);
    void fetchThemeSong();
    void stopThemeSong();
    Settings *m_settings;
    MpvPlayer *m_player;
    MpvPlayer *m_themePlayer;
    JellyfinClient m_api;
    QString m_userId, m_username, m_profileImage, m_title = "Home", m_message;
    QVariantList m_shelves, m_libraries;
    QString m_pageParent, m_pageSearch, m_pageCategory;
    QString m_pageType, m_pageSeries, m_filter = "All", m_sort = "Name";
    QVariantMap m_detail;
    int m_detailGeneration = 0;
    int m_themeGeneration = 0;
    QString m_themeContext, m_themeSongId;
    bool m_authenticating = false;
    bool m_themeEnabled = true;
    bool m_detailLoading = false, m_updatingUserData = false;
    QList<QVariantMap> m_history;
    int m_generation = 0, m_pending = 0, m_loaded = 0, m_total = 0, m_playGeneration = 0;
    bool m_negotiating = false, m_reportedStart = false;
    QVariantMap m_currentItem;
    QVariantMap m_playbackOptions;
    QVariantList m_segments;
    int m_segmentGeneration = 0;
    bool m_resumePaused = false;
    Playback::Plan m_plan;
    QTimer m_progress;
};
