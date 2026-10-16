#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <functional>
#include <cstring>

struct FixtureResponse { QByteArray body = "{}"; int status = 200; int delay = 0; };
struct FixtureRequest { QNetworkAccessManager::Operation operation; QNetworkRequest request; QByteArray body; };
class FixtureReply : public QNetworkReply {
public:
    FixtureReply(const QNetworkRequest &request, const FixtureResponse &response, QObject *parent)
        : QNetworkReply(parent), m_data(response.body) {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        open(QIODevice::ReadOnly);
        QTimer::singleShot(response.delay, this, [this, response] {
            if (response.status >= 400) setError(QNetworkReply::ContentAccessDenied, "Fixture HTTP error");
            setFinished(true); emit readyRead(); emit finished();
        });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return m_data.size() - m_position + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 max) override {
        const qint64 count = qMin(max, qint64(m_data.size()) - m_position);
        if (!count) return -1;
        std::memcpy(data, m_data.constData() + m_position, size_t(count)); m_position += count; return count;
    }
private:
    QByteArray m_data;
    qint64 m_position = 0;
};
class FakeNetwork : public QNetworkAccessManager {
public:
    std::function<FixtureResponse(const FixtureRequest &)> respond;
    QList<FixtureRequest> requests;
protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *body) override {
        FixtureRequest captured{operation, request, body ? body->readAll() : QByteArray()};
        requests.append(captured);
        return new FixtureReply(request, respond ? respond(captured) : FixtureResponse{}, this);
    }
};
