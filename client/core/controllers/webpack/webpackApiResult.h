#ifndef WEBPACKAPIRESULT_H
#define WEBPACKAPIRESULT_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUrl>

namespace webpack
{
    // How a call to webpack.ir /api/v1 ended. The gateway wraps every answer in
    // ApiResult<T> { success, data, error { code, message, validationErrors? }, correlationId }
    // (webpack docs/ai/API_REFERENCE.md section 1).
    enum class ApiOutcome {
        Ok,           // 2xx with success: true
        Rejected,     // a domain refusal with an error code (400, 402, 403, 404, 409)
        SignedOut,    // 401: no session, or the refresh failed
        RateLimited,  // 429
        Unreachable,  // DNS, TLS, timeout, connection refused
        ServerError,  // 5xx
        BadResponse   // not the expected envelope
    };

    struct ApiResult
    {
        ApiOutcome outcome = ApiOutcome::BadResponse;
        int httpStatus = 0;
        QJsonValue data;
        QString errorCode;
        QString message;
        QJsonObject validationErrors;
        QString correlationId;
        int retryAfterSeconds = 0;

        bool isOk() const { return outcome == ApiOutcome::Ok; }
        QJsonObject object() const { return data.toObject(); }
    };

    // Pure: builds an ApiResult from an HTTP status and body. httpStatus 0 means no answer
    // (transport failure); networkError is then a short, secret-free reason.
    ApiResult parseApiResult(int httpStatus, const QByteArray &body, const QByteArray &retryAfterHeader = {},
                             const QString &networkError = {});

    // A correlation id webpack accepts and echoes: 1 to 64 letters, digits or hyphens.
    QString newCorrelationId();
    bool isValidCorrelationId(const QString &id);

    // Only HTTPS, or plain HTTP to a loopback host when allowLoopbackHttp (development builds).
    bool isAllowedBaseUrl(const QUrl &url, bool allowLoopbackHttp);

    // The site's root, normalised to end with "/" ("https://webpack.ir/").
    QUrl normalisedBaseUrl(const QUrl &url);

    constexpr const char *defaultBaseUrl = "https://webpack.ir/";
} // namespace webpack

#endif // WEBPACKAPIRESULT_H
