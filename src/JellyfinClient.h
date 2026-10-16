#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QUrlQuery>
#include <functional>

class JellyfinClient : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonObject &, const QString &)>;
    explicit JellyfinClient(QString deviceId, QObject *parent = nullptr);
    void configure(const QUrl &server, const QString &token = {});
    QUrl server() const { return m_server; }
    QString token() const { return m_token; }
    QString deviceId() const { return m_deviceId; }
    QByteArray authorization() const;
    void get(const QString &path, const QUrlQuery &query, Callback callback);
    void post(const QString &path, const QJsonObject &body, Callback callback = {});
    void remove(const QString &path, const QUrlQuery &query, Callback callback = {});
    QString imageUrl(const QString &id, const QString &type, const QString &tag, int width) const;
signals:
    void unauthorized();
private:
    void send(const QByteArray &method, const QString &path, const QUrlQuery &query, const QJsonObject &body, Callback callback);
    QNetworkAccessManager m_network;
    QUrl m_server;
    QString m_token, m_deviceId;
    int m_generation = 0;
};
