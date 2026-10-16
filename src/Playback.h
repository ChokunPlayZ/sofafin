#pragma once
#include <QJsonObject>
#include <QUrl>

namespace Playback {
QUrl endpoint(const QUrl &server, const QString &path);
QUrl resourceUrl(const QUrl &server, const QUrl &resource);
bool sameOrigin(const QUrl &a, const QUrl &b);
QJsonObject deviceProfile(int bitrate);
QJsonObject request(const QString &userId, const QString &mode, int bitrate, qint64 startTicks);
struct Plan {
    QUrl url;
    QString mediaSourceId, sessionId, method, error;
    QJsonObject source;
    double offset = 0;
    bool valid() const { return error.isEmpty() && !url.isEmpty(); }
};
Plan negotiate(const QJsonObject &response, const QUrl &server, const QString &itemId,
               const QString &deviceId, const QString &token, const QString &mode, qint64 startTicks);
}
