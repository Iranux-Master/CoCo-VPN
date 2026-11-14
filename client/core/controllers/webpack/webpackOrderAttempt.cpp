#include "webpackOrderAttempt.h"

#include <QStringList>
#include <QUuid>

namespace webpack
{
    namespace
    {
        const QStringList chargeFreeCodes { "insufficient_balance", "price_changed", "not_found", "not_available",
                                            "invalid_request", "order_failed", "previous_failed", "idempotency_conflict" };
        const QStringList namedRefusals { "insufficient_balance", "price_changed", "not_found", "not_available", "order_failed",
                                          "order_pending_review", "previous_failed", "rate_limited", "invalid_request",
                                          "idempotency_conflict" };
    } // namespace

    QString OrderAttempt::keyFor(const QString &planPublicId, const QString &regionPublicId, const QString &operatingSystemId,
                                 const QString &amount, const QString &currencyCode)
    {
        const QString choice = QStringList { planPublicId, regionPublicId, operatingSystemId, amount, currencyCode }.join(QLatin1Char('|'));
        if (m_key.isEmpty() || (!m_pending && m_choice != choice)) {
            m_key = QStringLiteral("coco-") + QUuid::createUuid().toString(QUuid::Id128);
            m_choice = choice;
        }
        return m_key;
    }

    void OrderAttempt::after(const ApiResult &result)
    {
        if (result.isOk() || nothingCharged(result)) {
            m_key.clear();
            m_pending = false;
        } else {
            m_pending = true;
        }
    }

    void OrderAttempt::restore(const QString &key, const QString &choice, bool pending)
    {
        m_key = key;
        m_choice = choice;
        m_pending = pending;
    }

    bool OrderAttempt::nothingCharged(const ApiResult &result)
    {
        return result.outcome == ApiOutcome::Rejected && result.httpStatus < 500 && chargeFreeCodes.contains(result.errorCode);
    }

    bool OrderAttempt::reloadsPlans(const ApiResult &result)
    {
        return result.errorCode == QLatin1String("price_changed") || result.errorCode == QLatin1String("not_found")
                || result.errorCode == QLatin1String("not_available");
    }

    QString OrderAttempt::refusalKey(const ApiResult &result)
    {
        if (result.outcome == ApiOutcome::SignedOut) {
            return QStringLiteral("signed_out");
        }
        if (result.outcome == ApiOutcome::Unreachable) {
            return QStringLiteral("unreachable");
        }
        if (result.outcome == ApiOutcome::RateLimited) {
            return QStringLiteral("rate_limited");
        }
        return namedRefusals.contains(result.errorCode) ? result.errorCode : QStringLiteral("other");
    }
} // namespace webpack
