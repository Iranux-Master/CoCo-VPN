#include "webpackApiClient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace webpack
{
    ApiClient::Call ApiClient::makeCall(const QByteArray &verb, const QString &path, const QByteArray &body, bool hasBody,
                                        const RequestOptions &options, const Callback &callback)
    {
        Call call;
        call.verb = verb;
        call.path = path;
        call.body = body;
        call.hasBody = hasBody;
        call.options = options;
        call.callback = callback;
        return call;
    }

    ApiClient::ApiClient(QNetworkAccessManager *network, QObject *parent)
        : QObject(parent), m_network(network ? network : new QNetworkAccessManager(this)), m_baseUrl(QUrl(QString::fromLatin1(defaultBaseUrl)))
    {
        const QString version = QCoreApplication::applicationVersion();
        m_userAgent = QByteArrayLiteral("CoCoVPN/") + (version.isEmpty() ? QByteArrayLiteral("dev") : version.toUtf8());
    }

    bool ApiClient::setBaseUrl(const QUrl &url)
    {
        if (!isAllowedBaseUrl(url, m_allowLoopbackHttp)) {
            return false;
        }
        m_baseUrl = normalisedBaseUrl(url);
        return true;
    }

    QUrl ApiClient::urlFor(const QString &path) const
    {
        QString relative = path;
        while (relative.startsWith(QLatin1Char('/'))) {
            relative.remove(0, 1);
        }
        return m_baseUrl.resolved(QUrl(relative));
    }

    void ApiClient::get(const QString &path, const Callback &callback, const RequestOptions &options)
    {
        start(makeCall(QByteArrayLiteral("GET"), path, {}, false, options, callback));
    }

    void ApiClient::post(const QString &path, const QJsonObject &body, const Callback &callback, const RequestOptions &options)
    {
        start(makeCall(QByteArrayLiteral("POST"), path, QJsonDocument(body).toJson(QJsonDocument::Compact), true, options, callback));
    }

    void ApiClient::put(const QString &path, const QJsonObject &body, const Callback &callback, const RequestOptions &options)
    {
        start(makeCall(QByteArrayLiteral("PUT"), path, QJsonDocument(body).toJson(QJsonDocument::Compact), true, options, callback));
    }

    void ApiClient::deleteResource(const QString &path, const Callback &callback, const RequestOptions &options)
    {
        start(makeCall(QByteArrayLiteral("DELETE"), path, {}, false, options, callback));
    }

    void ApiClient::start(Call call)
    {
        call.correlationId = newCorrelationId();

        if (!call.options.requiresAccount) {
            send(call, QString());
            return;
        }

        const QString token = m_tokenSource ? m_tokenSource->accessToken() : QString();
        if (!token.isEmpty()) {
            send(call, token);
            return;
        }

        if (!m_tokenSource) {
            ApiResult result = parseApiResult(401, {});
            result.correlationId = call.correlationId;
            if (call.callback) {
                call.callback(result);
            }
            return;
        }

        // No access token yet (start-up, or it expired): renew it first.
        QPointer<ApiClient> self(this);
        call.retriedAfterRefresh = true;
        m_tokenSource->refreshAccessToken([self, call](bool ok) {
            if (!self) {
                return;
            }
            const QString renewed = ok && self->m_tokenSource ? self->m_tokenSource->accessToken() : QString();
            if (renewed.isEmpty()) {
                ApiResult result = parseApiResult(401, {});
                result.correlationId = call.correlationId;
                if (call.callback) {
                    call.callback(result);
                }
                return;
            }
            self->send(call, renewed);
        });
    }

    void ApiClient::send(const Call &call, const QString &accessToken)
    {
        QNetworkRequest request(urlFor(call.path));
        request.setRawHeader("Accept", "application/json");
        request.setRawHeader("X-Correlation-ID", call.correlationId.toLatin1());
        request.setRawHeader("User-Agent", m_userAgent);
        if (call.hasBody) {
            request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
        }
        if (!accessToken.isEmpty()) {
            request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toLatin1());
        }
        // A redirect could carry the bearer token to another host: never follow one.
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(call.options.timeoutMs);

        QNetworkReply *reply = call.hasBody ? m_network->sendCustomRequest(request, call.verb, call.body)
                                            : m_network->sendCustomRequest(request, call.verb);

        QPointer<ApiClient> self(this);
        connect(reply, &QNetworkReply::finished, this, [self, reply, call]() {
            if (!self) {
                reply->deleteLater();
                return;
            }
            self->finish(reply, call);
        });
    }

    void ApiClient::finish(QNetworkReply *reply, Call call)
    {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const QByteArray retryAfter = reply->rawHeader("Retry-After");

        ApiResult result;
        if (status == 0) {
            // Transport failure: say only which kind, never the URL or headers.
            result = parseApiResult(0, {}, {}, QStringLiteral("network error %1").arg(int(reply->error())));
        } else if (status >= 300 && status < 400) {
            result = parseApiResult(200, QByteArray());
            result.httpStatus = status;
            result.errorCode = QStringLiteral("unexpected_redirect");
        } else {
            result = parseApiResult(status, body, retryAfter);
        }
        if (result.correlationId.isEmpty()) {
            result.correlationId = call.correlationId;
        }

        if (result.outcome == ApiOutcome::SignedOut && call.options.requiresAccount && !call.retriedAfterRefresh
            && m_tokenSource) {
            call.retriedAfterRefresh = true;
            QPointer<ApiClient> self(this);
            m_tokenSource->refreshAccessToken([self, call, result](bool ok) {
                if (!self) {
                    return;
                }
                const QString renewed = ok && self->m_tokenSource ? self->m_tokenSource->accessToken() : QString();
                if (renewed.isEmpty()) {
                    if (call.callback) {
                        call.callback(result);
                    }
                    return;
                }
                self->send(call, renewed);
            });
            return;
        }

        if (!result.isOk()) {
            qWarning().noquote() << "webpack:" << call.verb << call.path.section(QLatin1Char('?'), 0, 0) << "->"
                                 << result.httpStatus << result.errorCode << "correlation" << result.correlationId;
        }

        if (call.callback) {
            call.callback(result);
        }
    }
} // namespace webpack
