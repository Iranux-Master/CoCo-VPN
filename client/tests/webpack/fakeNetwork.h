#ifndef WEBPACK_FAKENETWORK_H
#define WEBPACK_FAKENETWORK_H

#include <QBuffer>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQueue>
#include <QTimer>

// A QNetworkAccessManager that answers from a script, so no test calls the real site.
class FakeReply : public QNetworkReply
{
    Q_OBJECT

public:
    FakeReply(const QNetworkRequest &request, QNetworkAccessManager::Operation op, int status, const QByteArray &body,
              const QList<QPair<QByteArray, QByteArray>> &headers, QObject *parent)
        : QNetworkReply(parent), m_body(body)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        if (status > 0) {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        } else {
            setError(QNetworkReply::ConnectionRefusedError, QStringLiteral("refused"));
        }
        for (const auto &header : headers) {
            setRawHeader(header.first, header.second);
        }
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this]() {
            emit readyRead();
            emit finished();
        });
    }

    void abort() override {}
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QIODevice::bytesAvailable(); }
    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 count = qMin<qint64>(maxSize, m_body.size() - m_offset);
        if (count <= 0) {
            return -1;
        }
        memcpy(data, m_body.constData() + m_offset, size_t(count));
        m_offset += count;
        return count;
    }

private:
    QByteArray m_body;
    qint64 m_offset = 0;
};

class FakeNetwork : public QNetworkAccessManager
{
    Q_OBJECT

public:
    struct Answer
    {
        QString pathSuffix;   // matched against the end of the request path
        int status = 200;
        QByteArray body;
        QList<QPair<QByteArray, QByteArray>> headers;
    };

    struct Seen
    {
        QByteArray verb;
        QString path;
        QByteArray authorization;
        QByteArray correlationId;
        QByteArray body;
    };

    void queue(const QString &pathSuffix, int status, const QByteArray &body, const QList<QPair<QByteArray, QByteArray>> &headers = {})
    {
        m_answers.append({ pathSuffix, status, body, headers });
    }

    QList<Seen> seen;

    static QByteArray ok(const QByteArray &dataJson)
    {
        return "{\"success\":true,\"data\":" + dataJson + ",\"error\":null,\"correlationId\":\"srv-1\"}";
    }
    static QByteArray fail(const QByteArray &code, const QByteArray &message = "refused")
    {
        return "{\"success\":false,\"data\":null,\"error\":{\"code\":\"" + code + "\",\"message\":\"" + message
                + "\"},\"correlationId\":\"srv-2\"}";
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        Seen s;
        s.verb = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
        s.path = request.url().path();
        s.authorization = request.rawHeader("Authorization");
        s.correlationId = request.rawHeader("X-Correlation-ID");
        if (outgoingData) {
            s.body = outgoingData->readAll();
        }
        seen.append(s);

        for (int i = 0; i < m_answers.size(); ++i) {
            if (s.path.endsWith(m_answers.at(i).pathSuffix)) {
                const Answer answer = m_answers.takeAt(i);
                return new FakeReply(request, op, answer.status, answer.body, answer.headers, this);
            }
        }
        return new FakeReply(request, op, 404, fail("not_found"), {}, this);
    }

private:
    QList<Answer> m_answers;
};

#endif // WEBPACK_FAKENETWORK_H
