#ifndef WEBPACKSHOP_H
#define WEBPACKSHOP_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <functional>

#include "webpackApiResult.h"

namespace webpack
{
    class ApiClient;

    struct PlanRegion
    {
        QString regionPublicId;
        QString displayName;
        QString city;
        QString countryCode;
    };

    struct PlanSystem
    {
        QString operatingSystemId;
        QString displayName;
        bool isDefault = false;
    };

    struct Plan
    {
        QString planPublicId;
        QString displayName;
        QString specifications;
        bool hasPrice = false;  // the caller's own quote in the wallet currency
        double priceAmount = 0;
        QString priceCurrency;
        int periodMonths = 1;
        int graceDays = 0;
        int deleteAfterPowerOffDays = 0;
        QList<PlanRegion> regions;
        QList<PlanSystem> systems;
    };

    struct Wallet
    {
        double balance = 0;
        QString currencyCode;
    };

    struct Checkout
    {
        QString paymentIntentPublicId;
        double amount = 0;
        QString currencyCode;
        QUrl checkoutUrl;
        QDateTime expiresAtUtc;
    };

    // Pure helpers (tested).
    QList<Plan> parsePlans(const QJsonValue &pagedData);
    // Systems CoCo's installer supports (Ubuntu and Debian); keeps the plan's default first.
    QList<PlanSystem> supportedSystems(const QList<PlanSystem> &systems);
    // How much to top up so the balance covers the price: toman rounded up to 1,000, dollars and
    // euros up to the cent. 0 when the balance is enough.
    double shortfall(double price, double balance, const QString &currencyCode);
    // Gateways that take this wallet currency, best first (API_REFERENCE section 3.3).
    QStringList gatewaysFor(const QString &currencyCode);
    // A JSON-safe decimal string for money ("150000", "4.99").
    QString moneyString(double amount, const QString &currencyCode);

    // Plans, wallet and top-up for the in-app purchase. The bank page always opens in the system
    // browser; CoCo then watches the balance until it covers the price.
    class Shop : public QObject
    {
        Q_OBJECT

    public:
        using PlansDone = std::function<void(const ApiResult &result, const QList<Plan> &plans)>;
        using WalletDone = std::function<void(const ApiResult &result, const Wallet &wallet)>;
        using CheckoutDone = std::function<void(const ApiResult &result, const Checkout &checkout)>;

        explicit Shop(ApiClient *api, QObject *parent = nullptr);

        void loadPlans(const PlansDone &done);
        void loadWallet(const WalletDone &done);
        void startCheckout(double amount, const QString &currencyCode, const QString &gatewayCode, const CheckoutDone &done);

        // Polls the balance (5 s for two minutes, then 15 s) until it reaches target or until deadline.
        void watchBalance(double target, const QDateTime &deadline);
        void checkBalanceNow();
        void stopWatching();
        bool isWatching() const { return m_watchTimer.isActive(); }

    signals:
        void balanceChanged(double balance, const QString &currencyCode);
        void balanceReached();
        void watchExpired();

    private:
        void pollOnce();

        ApiClient *m_api;
        QString m_checkoutKey;
        QString m_checkoutKeyFor;
        bool m_checkoutPending = false;

        QTimer m_watchTimer;
        double m_watchTarget = 0;
        QDateTime m_watchStarted;
        QDateTime m_watchDeadline;
        bool m_polling = false;
    };
} // namespace webpack

#endif // WEBPACKSHOP_H
