#include "webpackApiResult.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUuid>

namespace webpack
{
    namespace
    {
        ApiOutcome outcomeForFailure(int httpStatus)
        {
            if (httpStatus == 401) {
                return ApiOutcome::SignedOut;
            }
            if (httpStatus == 429) {
                return ApiOutcome::RateLimited;
            }
            if (httpStatus >= 500) {
                return ApiOutcome::ServerError;
            }
            return ApiOutcome::Rejected;
        }

        int parseRetryAfter(const QByteArray &header)
        {
            bool ok = false;
            const int seconds = header.trimmed().toInt(&ok);
            if (!ok || seconds < 0) {
                return 0;
            }
            return qMin(seconds, 600);
        }
    } // namespace

    ApiResult parseApiResult(int httpStatus, const QByteArray &body, const QByteArray &retryAfterHeader,
                             const QString &networkError)
    {
        ApiResult result;
        result.httpStatus = httpStatus;
        result.retryAfterSeconds = parseRetryAfter(retryAfterHeader);

        if (httpStatus == 0) {
            result.outcome = ApiOutcome::Unreachable;
            result.errorCode = QStringLiteral("unreachable");
            result.message = networkError;
            return result;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const bool isEnvelope = parseError.error == QJsonParseError::NoError && document.isObject()
                && document.object().contains(QStringLiteral("success"));

        if (!isEnvelope) {
            // A proxy page, an empty 502, or a rate-limit answer without a body.
            if (httpStatus >= 200 && httpStatus < 300) {
                result.outcome = ApiOutcome::BadResponse;
                result.errorCode = QStringLiteral("bad_response");
            } else {
                result.outcome = outcomeForFailure(httpStatus);
                result.errorCode = httpStatus == 429 ? QStringLiteral("rate_limited")
                        : httpStatus == 401          ? QStringLiteral("invalid_token")
                                                     : QStringLiteral("http_%1").arg(httpStatus);
            }
            return result;
        }

        const QJsonObject envelope = document.object();
        result.correlationId = envelope.value(QStringLiteral("correlationId")).toString();
        result.data = envelope.value(QStringLiteral("data"));

        const bool success = envelope.value(QStringLiteral("success")).toBool();
        const QJsonObject error = envelope.value(QStringLiteral("error")).toObject();
        result.errorCode = error.value(QStringLiteral("code")).toString();
        result.message = error.value(QStringLiteral("message")).toString();
        result.validationErrors = error.value(QStringLiteral("validationErrors")).toObject();

        if (success && httpStatus >= 200 && httpStatus < 300) {
            result.outcome = ApiOutcome::Ok;
            return result;
        }

        if (httpStatus >= 200 && httpStatus < 300) {
            // success: false with a 2xx status is not something the gateway sends.
            result.outcome = ApiOutcome::BadResponse;
            if (result.errorCode.isEmpty()) {
                result.errorCode = QStringLiteral("bad_response");
            }
            return result;
        }

        result.outcome = outcomeForFailure(httpStatus);
        if (result.errorCode.isEmpty()) {
            result.errorCode = httpStatus == 429 ? QStringLiteral("rate_limited") : QStringLiteral("http_%1").arg(httpStatus);
        }
        return result;
    }

    QString newCorrelationId()
    {
        return QStringLiteral("coco-") + QUuid::createUuid().toString(QUuid::Id128);
    }

    bool isValidCorrelationId(const QString &id)
    {
        static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9-]{1,64}$"));
        return pattern.match(id).hasMatch();
    }

    bool isAllowedBaseUrl(const QUrl &url, bool allowLoopbackHttp)
    {
        if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()) {
            return false;
        }
        const QString scheme = url.scheme().toLower();
        if (scheme == QLatin1String("https")) {
            return true;
        }
        if (scheme != QLatin1String("http") || !allowLoopbackHttp) {
            return false;
        }
        const QString host = url.host().toLower();
        if (host == QLatin1String("localhost")) {
            return true;
        }
        const QHostAddress address(host);
        return !address.isNull() && address.isLoopback();
    }

    QUrl normalisedBaseUrl(const QUrl &url)
    {
        QUrl base = url.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment);
        QString path = base.path();
        if (!path.endsWith(QLatin1Char('/'))) {
            path += QLatin1Char('/');
        }
        base.setPath(path);
        return base;
    }
} // namespace webpack
