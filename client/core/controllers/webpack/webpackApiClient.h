#ifndef WEBPACKAPICLIENT_H
#define WEBPACKAPICLIENT_H

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>

#include "webpackApiResult.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace webpack
{
    // Supplies the device access token and renews it. Implemented by WebpackSession.
    class TokenSource
    {
    public:
        virtual ~TokenSource() = default;
        virtual QString accessToken() const = 0;
        // Renews the access token (one refresh at a time); done(true) when a new one is ready.
        virtual void refreshAccessToken(const std::function<void(bool ok)> &done) = 0;
    };

    struct RequestOptions
    {
        bool requiresAccount = true;
        int timeoutMs = 30000;
    };

    // HTTPS client for webpack.ir /api/v1. One instance for the app. Every call is asynchronous;
    // the callback runs on the thread that owns the client. It sends X-Correlation-ID, the bearer
    // token, refreshes once on 401, never follows redirects (the bearer must not leave the site)
    // and never logs bodies, tokens or the Authorization header.
    class ApiClient : public QObject
    {
        Q_OBJECT

    public:
        using Callback = std::function<void(const ApiResult &result)>;

        explicit ApiClient(QNetworkAccessManager *network = nullptr, QObject *parent = nullptr);

        bool setBaseUrl(const QUrl &url);
        QUrl baseUrl() const { return m_baseUrl; }
        void setAllowLoopbackHttp(bool allow) { m_allowLoopbackHttp = allow; }
        void setTokenSource(TokenSource *source) { m_tokenSource = source; }
        void setUserAgent(const QByteArray &userAgent) { m_userAgent = userAgent; }

        // path is relative to the site root, for example "api/v1/vps/plans".
        void get(const QString &path, const Callback &callback, const RequestOptions &options = {});
        void post(const QString &path, const QJsonObject &body, const Callback &callback, const RequestOptions &options = {});
        void put(const QString &path, const QJsonObject &body, const Callback &callback, const RequestOptions &options = {});
        void deleteResource(const QString &path, const Callback &callback, const RequestOptions &options = {});

        QUrl urlFor(const QString &path) const;

    private:
        struct Call
        {
            QByteArray verb;
            QString path;
            QByteArray body;
            bool hasBody = false;
            RequestOptions options;
            Callback callback;
            QString correlationId;
            bool retriedAfterRefresh = false;
        };

        static Call makeCall(const QByteArray &verb, const QString &path, const QByteArray &body, bool hasBody,
                             const RequestOptions &options, const Callback &callback);
        void start(Call call);
        void send(const Call &call, const QString &accessToken);
        void finish(QNetworkReply *reply, Call call);

        QNetworkAccessManager *m_network = nullptr;
        QUrl m_baseUrl;
        bool m_allowLoopbackHttp = false;
        TokenSource *m_tokenSource = nullptr;
        QByteArray m_userAgent;
    };
} // namespace webpack

#endif // WEBPACKAPICLIENT_H
