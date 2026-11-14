#include "webpackLoopback.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace webpack
{
    namespace
    {
        constexpr int maxRequestBytes = 8192;
        constexpr int connectionTimeoutMs = 5000;

        QString base64Url(const QByteArray &bytes)
        {
            return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
        }

        QString percentDecode(QByteArray part)
        {
            part.replace('+', ' ');
            return QUrl::fromPercentEncoding(part);
        }
    } // namespace

    Pkce Pkce::create()
    {
        QByteArray random(32, Qt::Uninitialized);
        QRandomGenerator *generator = QRandomGenerator::system();
        for (int i = 0; i < random.size(); ++i) {
            random[i] = char(generator->bounded(256));
        }
        Pkce pkce;
        pkce.verifier = base64Url(random);
        pkce.challenge = challengeFor(pkce.verifier);
        return pkce;
    }

    QString Pkce::challengeFor(const QString &verifier)
    {
        return base64Url(QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256));
    }

    std::optional<QMap<QString, QString>> parseLoopbackRequest(const QByteArray &requestHead)
    {
        const int lineEnd = requestHead.indexOf("\r\n");
        const QByteArray line = lineEnd < 0 ? requestHead : requestHead.left(lineEnd);
        const QList<QByteArray> parts = line.split(' ');
        if (parts.size() < 2 || parts.at(0) != "GET") {
            return std::nullopt;
        }

        const QByteArray target = parts.at(1);
        const int question = target.indexOf('?');
        const QByteArray path = question < 0 ? target : target.left(question);
        if (path != QByteArray(LoopbackReceiver::callbackPath)) {
            return std::nullopt;
        }

        QMap<QString, QString> query;
        if (question >= 0) {
            const QList<QByteArray> pairs = target.mid(question + 1).split('&');
            for (const QByteArray &pair : pairs) {
                if (pair.isEmpty()) {
                    continue;
                }
                const int equals = pair.indexOf('=');
                const QString key = percentDecode(equals < 0 ? pair : pair.left(equals));
                const QString value = equals < 0 ? QString() : percentDecode(pair.mid(equals + 1));
                if (!query.contains(key)) {
                    query.insert(key, value);
                }
            }
        }
        return query;
    }

    LoopbackReceiver::LoopbackReceiver(QObject *parent) : QObject(parent)
    {
    }

    LoopbackReceiver::~LoopbackReceiver()
    {
        stop();
    }

    bool LoopbackReceiver::start()
    {
        stop();
        m_server = new QTcpServer(this);
        m_server->setMaxPendingConnections(8);
        if (!m_server->listen(QHostAddress::LocalHost, 0)) {
            delete m_server;
            m_server = nullptr;
            return false;
        }

        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            while (m_server && m_server->hasPendingConnections()) {
                QTcpSocket *socket = m_server->nextPendingConnection();
                QPointer<QTcpSocket> guard(socket);
                auto buffer = QSharedPointer<QByteArray>::create();

                QTimer::singleShot(connectionTimeoutMs, socket, [guard]() {
                    if (guard) {
                        guard->abort();
                    }
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
                    buffer->append(socket->read(maxRequestBytes - buffer->size()));
                    if (!buffer->contains("\r\n") && buffer->size() < maxRequestBytes) {
                        return;
                    }
                    disconnect(socket, &QTcpSocket::readyRead, this, nullptr);

                    const auto query = parseLoopbackRequest(*buffer);
                    const QByteArray body = query
                            ? QByteArrayLiteral("<!doctype html><meta charset=\"utf-8\"><title>CoCo VPN</title>"
                                                "<p style=\"font-family:sans-serif;margin:3em\">You can close this tab and go back to CoCo VPN.<br>"
                                                "\xd9\x85\xdb\x8c\xe2\x80\x8c\xd8\xaa\xd9\x88\xd8\xa7\xd9\x86\xdb\x8c\xd8\xaf "
                                                "\xd8\xa7\xdb\x8c\xd9\x86 \xd8\xb2\xd8\xa8\xd8\xa7\xd9\x86\xd9\x87 \xd8\xb1\xd8\xa7 "
                                                "\xd8\xa8\xd8\xa8\xd9\x86\xd8\xaf\xdb\x8c\xd8\xaf \xd9\x88 \xd8\xa8\xd9\x87 CoCo VPN "
                                                "\xd8\xa8\xd8\xb1\xda\xaf\xd8\xb1\xd8\xaf\xdb\x8c\xd8\xaf.</p>")
                            : QByteArrayLiteral("Not found");
                    QByteArray head = query ? QByteArrayLiteral("HTTP/1.1 200 OK\r\n") : QByteArrayLiteral("HTTP/1.1 404 Not Found\r\n");
                    head += "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: "
                            + QByteArray::number(body.size()) + "\r\n\r\n";
                    socket->write(head + body);
                    socket->disconnectFromHost();

                    if (query) {
                        emit returned(*query);
                    }
                });
            }
        });
        return true;
    }

    void LoopbackReceiver::stop()
    {
        if (m_server) {
            m_server->close();
            m_server->deleteLater();
            m_server = nullptr;
        }
    }

    quint16 LoopbackReceiver::port() const
    {
        return m_server ? m_server->serverPort() : 0;
    }
} // namespace webpack
