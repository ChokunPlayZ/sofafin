#include "JellyfinClient.h"
#include "Playback.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkReply>

JellyfinClient::JellyfinClient(QString deviceId, QObject *parent, QNetworkAccessManager *transport)
    : QObject(parent), m_network(transport ? transport : new QNetworkAccessManager(this)), m_deviceId(std::move(deviceId)) {}
void JellyfinClient::configure(const QUrl &server, const QString &token) { m_server = server; m_token = token; ++m_generation; }
QByteArray JellyfinClient::authorization() const {
    auto result = QByteArray("MediaBrowser Client=\"JF-Gamescope\", Device=\"Steam Deck\", DeviceId=\"") + m_deviceId.toUtf8() + "\", Version=\"0.1.0\"";
    if (!m_token.isEmpty()) result += ", Token=\"" + m_token.toUtf8() + '"';
    return result;
}
void JellyfinClient::get(const QString &p, const QUrlQuery &q, Callback cb) { send("GET", p, q, {}, std::move(cb)); }
void JellyfinClient::post(const QString &p, const QJsonObject &b, Callback cb) { send("POST", p, {}, b, std::move(cb)); }
void JellyfinClient::remove(const QString &p, const QUrlQuery &q, Callback cb) { send("DELETE", p, q, {}, std::move(cb)); }
void JellyfinClient::send(const QByteArray &method, const QString &path, const QUrlQuery &query, const QJsonObject &body, Callback cb) {
    auto url = Playback::endpoint(m_server, path);
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", authorization());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    req.setTransferTimeout(15000);
    auto *reply = method == "GET" ? m_network->get(req) : method == "DELETE" ? m_network->deleteResource(req)
        : m_network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, cb = std::move(cb), generation] {
        reply->deleteLater();
        if (generation != m_generation) return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error;
        if (status == 401 || status == 403) error = "Sign-in failed or access was denied. Check your account and server permissions.";
        else if (reply->error() != QNetworkReply::NoError) error = status ? QString("The server returned HTTP %1.").arg(status) : "Could not reach the server. Check its address and your connection.";
        QJsonParseError parse;
        const QByteArray data = reply->readAll();
        auto doc = QJsonDocument::fromJson(data, &parse);
        if (error.isEmpty() && !data.isEmpty() && parse.error != QJsonParseError::NoError) error = "The server returned an invalid response. Check the Jellyfin base URL.";
        QJsonObject result = doc.isArray() ? QJsonObject{{"Items", doc.array()}} : doc.object();
        if (cb) cb(result, error);
        if (status == 401 && !m_token.isEmpty()) emit unauthorized();
    });
}
QString JellyfinClient::imageUrl(const QString &id, const QString &type, const QString &tag, int width) const {
    if (id.isEmpty() || tag.isEmpty()) return {};
    auto url = Playback::endpoint(m_server, "Items/" + id + "/Images/" + type);
    QUrlQuery q;
    q.addQueryItem("tag", tag);
    q.addQueryItem("maxWidth", QString::number(width));
    q.addQueryItem("quality", "85");
    q.addQueryItem("api_key", m_token);
    url.setQuery(q);
    return url.toString(QUrl::FullyEncoded);
}
QString JellyfinClient::userImageUrl(const QString &id, const QString &tag) const {
    if (id.isEmpty() || tag.isEmpty()) return {};
    auto url = Playback::endpoint(m_server, "Users/" + QString::fromLatin1(QUrl::toPercentEncoding(id)) + "/Images/Primary");
    QUrlQuery q;
    q.addQueryItem("tag", tag);
    q.addQueryItem("maxWidth", "96");
    q.addQueryItem("api_key", m_token);
    url.setQuery(q);
    return url.toString(QUrl::FullyEncoded);
}
