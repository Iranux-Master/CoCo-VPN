#include "webpackShop.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include <cmath>

#include "webpackApiClient.h"

namespace webpack
{
    namespace
    {
        bool isToman(const QString &currencyCode)
        {
            return currencyCode.compare(QLatin1String("IRT"), Qt::CaseInsensitive) == 0;
        }
    } // namespace

    QList<Plan> parsePlans(const QJsonValue &pagedData)
    {
        QList<Plan> plans;
        const QJsonArray items = pagedData.isArray() ? pagedData.toArray() : pagedData.toObject().value("items").toArray();
        for (const QJsonValue &value : items) {
            const QJsonObject item = value.toObject();
            Plan plan;
            plan.planPublicId = item.value("planPublicId").toString();
            plan.displayName = item.value("displayName").toString();
            plan.specifications = item.value("specifications").toString();
            const QJsonObject price = item.value("price").toObject();
            if (!price.isEmpty() && price.value("amount").isDouble()) {
                plan.hasPrice = true;
                plan.priceAmount = price.value("amount").toDouble();
                plan.priceCurrency = price.value("currencyCode").toString();
            }
            const QJsonObject terms = item.value("billingTerms").toObject();
            plan.periodMonths = terms.value("periodMonths").toInt(1);
            plan.graceDays = terms.value("graceDays").toInt();
            plan.deleteAfterPowerOffDays = terms.value("deleteAfterPowerOffDays").toInt();
            for (const QJsonValue &r : item.value("regions").toArray()) {
                const QJsonObject region = r.toObject();
                plan.regions.append({ region.value("regionPublicId").toString(), region.value("displayName").toString(),
                                      region.value("city").toString(), region.value("countryCode").toString() });
            }
            for (const QJsonValue &s : item.value("operatingSystems").toArray()) {
                const QJsonObject system = s.toObject();
                plan.systems.append({ system.value("operatingSystemId").toString(), system.value("displayName").toString(),
                                      system.value("isDefault").toBool() });
            }
            if (!plan.planPublicId.isEmpty()) {
                plans.append(plan);
            }
        }
        return plans;
    }

    QList<PlanSystem> supportedSystems(const QList<PlanSystem> &systems)
    {
        QList<PlanSystem> supported;
        for (const PlanSystem &system : systems) {
            const QString name = system.displayName.toLower();
            if (name.contains(QLatin1String("ubuntu")) || name.contains(QLatin1String("debian"))) {
                if (system.isDefault) {
                    supported.prepend(system);
                } else {
                    supported.append(system);
                }
            }
        }
        return supported;
    }

    double shortfall(double price, double balance, const QString &currencyCode)
    {
        const double missing = price - balance;
        if (missing <= 0.0000001) {
            return 0;
        }
        if (isToman(currencyCode)) {
            return std::ceil(missing / 1000.0 - 1e-9) * 1000.0;
        }
        return std::ceil(missing * 100.0 - 1e-6) / 100.0;
    }

    QStringList gatewaysFor(const QString &currencyCode)
    {
        if (isToman(currencyCode)) {
            return { QStringLiteral("zarinpal") };
        }
        return { QStringLiteral("stripe"), QStringLiteral("paypal"), QStringLiteral("nowpayments") };
    }

    QString moneyString(double amount, const QString &currencyCode)
    {
        if (isToman(currencyCode)) {
            return QString::number(std::llround(amount));
        }
        return QString::number(amount, 'f', 2);
    }

    Shop::Shop(ApiClient *api, QObject *parent) : QObject(parent), m_api(api)
    {
        m_watchTimer.setSingleShot(true);
        connect(&m_watchTimer, &QTimer::timeout, this, &Shop::pollOnce);
    }

    void Shop::loadPlans(const PlansDone &done)
    {
        m_api->get(QStringLiteral("api/v1/vps/plans?page=1&pageSize=100"), [done](const ApiResult &result) {
            done(result, result.isOk() ? parsePlans(result.data) : QList<Plan> {});
        });
    }

    void Shop::loadWallet(const WalletDone &done)
    {
        m_api->get(QStringLiteral("api/v1/wallet/balance"), [done](const ApiResult &result) {
            Wallet wallet;
            if (result.isOk()) {
                wallet.balance = result.object().value("currentBalance").toDouble();
                wallet.currencyCode = result.object().value("currencyCode").toString();
            }
            done(result, wallet);
        });
    }

    void Shop::startCheckout(double amount, const QString &currencyCode, const QString &gatewayCode, const CheckoutDone &done)
    {
        const QString amountText = moneyString(amount, currencyCode);
        const QString choice = amountText + QLatin1Char('|') + currencyCode + QLatin1Char('|') + gatewayCode;
        // Re-calling with the same key returns the unexpired checkout and never starts a second one.
        if (m_checkoutKey.isEmpty() || (!m_checkoutPending && m_checkoutKeyFor != choice)) {
            m_checkoutKey = QStringLiteral("coco-topup-") + QUuid::createUuid().toString(QUuid::Id128);
            m_checkoutKeyFor = choice;
        }

        QJsonObject body { { "amount", isToman(currencyCode) ? QJsonValue(double(std::llround(amount))) : QJsonValue(amountText.toDouble()) },
                           { "idempotencyKey", m_checkoutKey },
                           { "currencyCode", currencyCode },
                           { "gatewayCode", gatewayCode } };
        QPointer<Shop> self(this);
        m_api->post(QStringLiteral("api/v1/wallet/payment-checkouts"), body, [self, done](const ApiResult &result) {
            Checkout checkout;
            if (result.isOk()) {
                const QJsonObject data = result.object();
                checkout.paymentIntentPublicId = data.value("paymentIntentPublicId").toString();
                checkout.amount = data.value("amount").toDouble();
                checkout.currencyCode = data.value("currencyCode").toString();
                checkout.checkoutUrl = QUrl(data.value("checkoutUrl").toString());
                checkout.expiresAtUtc = QDateTime::fromString(data.value("expiresAtUtc").toString(), Qt::ISODateWithMs);
            }
            if (self) {
                // An unclear answer keeps the key, so a retry cannot open a second payment.
                const bool unclear = result.outcome == ApiOutcome::Unreachable || result.outcome == ApiOutcome::ServerError
                        || result.outcome == ApiOutcome::RateLimited;
                self->m_checkoutPending = unclear;
                if (!unclear && !result.isOk()) {
                    self->m_checkoutKey.clear();
                }
                if (result.errorCode == QLatin1String("payment_checkout_expired")) {
                    self->m_checkoutKey.clear();
                }
            }
            // A checkout link must be HTTPS before it is opened.
            if (result.isOk() && checkout.checkoutUrl.scheme() != QLatin1String("https")) {
                ApiResult bad = result;
                bad.outcome = ApiOutcome::BadResponse;
                bad.errorCode = QStringLiteral("bad_response");
                done(bad, {});
                return;
            }
            done(result, checkout);
        });
    }

    void Shop::watchBalance(double target, const QDateTime &deadline)
    {
        m_watchTarget = target;
        m_watchStarted = QDateTime::currentDateTimeUtc();
        const QDateTime cap = m_watchStarted.addSecs(60 * 60);
        m_watchDeadline = deadline.isValid() && deadline < cap ? deadline.addSecs(120) : cap;
        m_watchTimer.start(5000);
    }

    void Shop::checkBalanceNow()
    {
        if (isWatching()) {
            m_watchTimer.stop();
            pollOnce();
        }
    }

    void Shop::stopWatching()
    {
        m_watchTimer.stop();
        m_watchTarget = 0;
    }

    void Shop::pollOnce()
    {
        if (m_polling) {
            return;
        }
        m_polling = true;
        QPointer<Shop> self(this);
        loadWallet([self](const ApiResult &result, const Wallet &wallet) {
            if (!self) {
                return;
            }
            self->m_polling = false;
            if (self->m_watchTarget <= 0) {
                return; // stopped meanwhile
            }
            if (result.isOk()) {
                emit self->balanceChanged(wallet.balance, wallet.currencyCode);
                if (wallet.balance + 0.0000001 >= self->m_watchTarget) {
                    self->stopWatching();
                    emit self->balanceReached();
                    return;
                }
            }
            const QDateTime now = QDateTime::currentDateTimeUtc();
            if (now >= self->m_watchDeadline) {
                self->stopWatching();
                emit self->watchExpired();
                return;
            }
            int intervalMs = self->m_watchStarted.secsTo(now) < 120 ? 5000 : 15000;
            if (result.outcome == ApiOutcome::RateLimited && result.retryAfterSeconds > 0) {
                intervalMs = qMax(intervalMs, result.retryAfterSeconds * 1000);
            }
            self->m_watchTimer.start(intervalMs);
        });
    }
} // namespace webpack
