#ifndef WEBPACKLOOPBACK_H
#define WEBPACKLOOPBACK_H

#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QString>

#include <optional>

class QTcpServer;

namespace webpack
{
    // PKCE (RFC 7636, S256) for the Google sign-in through the system browser.
    struct Pkce
    {
        QString verifier;  // 43 base64url characters (32 random bytes)
        QString challenge; // base64url(SHA-256(verifier)), no padding
        static Pkce create();
        static QString challengeFor(const QString &verifier);
    };

    // Pure: the query of "GET /webpack-signin?..." from the first line of an HTTP request, or nothing
    // for any other method or path.
    std::optional<QMap<QString, QString>> parseLoopbackRequest(const QByteArray &requestHead);

    // Listens on 127.0.0.1 (a free port) for the browser's return from the webpack Google sign-in
    // page (RFC 8252). Emits returned() for every well-formed hit on /webpack-signin; the session keeps
    // waiting until the hit names its own request id. Answers every connection with a small page.
    class LoopbackReceiver : public QObject
    {
        Q_OBJECT

    public:
        static constexpr const char *callbackPath = "/webpack-signin";

        explicit LoopbackReceiver(QObject *parent = nullptr);
        ~LoopbackReceiver() override;

        bool start();
        void stop();
        quint16 port() const;

    signals:
        void returned(const QMap<QString, QString> &query);

    private:
        QTcpServer *m_server = nullptr;
    };
} // namespace webpack

#endif // WEBPACKLOOPBACK_H
