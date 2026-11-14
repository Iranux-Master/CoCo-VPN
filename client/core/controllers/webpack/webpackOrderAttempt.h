#ifndef WEBPACKORDERATTEMPT_H
#define WEBPACKORDERATTEMPT_H

#include <QString>

#include "webpackApiResult.h"

namespace webpack
{
    // The idempotency key of the order about to be placed (a port of the webpack desktop app's
    // OrderAttempt, issue 1212). A key is renewed only after an answer proving nothing was charged
    // under it, or after the order succeeded. Any other answer (unreachable, rate limited, 5xx,
    // order_pending_review, an unknown code) may hide an order already charged, so the key is kept
    // and the next press sends the same key whatever the person chose since: the site then answers
    // with that order instead of charging again.
    class OrderAttempt
    {
    public:
        QString keyFor(const QString &planPublicId, const QString &regionPublicId, const QString &operatingSystemId,
                       const QString &amount, const QString &currencyCode);
        void after(const ApiResult &result);
        bool isPending() const { return m_pending; }
        QString currentKey() const { return m_key; }

        // Restores a kept key (persisted purchase) after a restart.
        void restore(const QString &key, const QString &choice, bool pending);
        QString currentChoice() const { return m_choice; }

        // Refusals given before any charge under the key, or that close it for good.
        static bool nothingCharged(const ApiResult &result);
        // The price or the offer changed: read the plans and the balance again.
        static bool reloadsPlans(const ApiResult &result);
        // A stable key for the message the page shows ("Buy.Refused.<code>").
        static QString refusalKey(const ApiResult &result);

    private:
        QString m_key;
        QString m_choice;
        bool m_pending = false;
    };
} // namespace webpack

#endif // WEBPACKORDERATTEMPT_H
