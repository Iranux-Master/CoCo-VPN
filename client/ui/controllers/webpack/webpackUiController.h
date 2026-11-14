#ifndef WEBPACKUICONTROLLER_H
#define WEBPACKUICONTROLLER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "core/controllers/webpack/webpackApiResult.h"
#include "core/controllers/webpack/webpackShop.h"

class InstallUiController;

namespace webpack
{
    class Integration;
}

// QML adapter ("WebpackController") for buying a server from webpack.ir and configuring it.
// Logic lives in core/controllers/webpack; this class only turns it into properties, calls and
// translated messages.
class WebpackUiController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool signedIn READ signedIn NOTIFY accountChanged)
    Q_PROPERTY(bool offline READ offline NOTIFY accountChanged)
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY accountChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString walletText READ walletText NOTIFY walletChanged)
    Q_PROPERTY(QString walletCurrency READ walletCurrency NOTIFY walletChanged)
    Q_PROPERTY(QVariantList plans READ plans NOTIFY plansChanged)
    Q_PROPERTY(QStringList gateways READ gateways NOTIFY walletChanged)
    Q_PROPERTY(bool waitingForPayment READ waitingForPayment NOTIFY paymentChanged)
    Q_PROPERTY(QString paymentText READ paymentText NOTIFY paymentChanged)
    Q_PROPERTY(QVariantList purchases READ purchases NOTIFY purchasesChanged)
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(bool hasPendingOrder READ hasPendingOrder NOTIFY purchasesChanged)

public:
    WebpackUiController(webpack::Integration *integration, InstallUiController *installUiController, QObject *parent = nullptr);

    bool signedIn() const;
    bool offline() const;
    QString accountEmail() const;
    bool busy() const { return m_busy > 0; }
    QString walletText() const;
    QString walletCurrency() const { return m_wallet.currencyCode; }
    QVariantList plans() const;
    QStringList gateways() const;
    bool waitingForPayment() const;
    QString paymentText() const { return m_paymentText; }
    QVariantList purchases() const;
    QVariantList servers() const;
    bool hasPendingOrder() const;

public slots:
    void signIn(const QString &email, const QString &password);
    void signInWithGoogle();
    void cancelGoogleSignIn();
    void signOut();
    void openAccountOnWebsite();

    void loadShop();
    // Buys with the wallet; when it is short, opens the gateway for the shortfall and buys once
    // the money arrives. gatewayIndex picks from gateways.
    void buy(const QString &planPublicId, const QString &regionPublicId, const QString &operatingSystemId, int gatewayIndex);
    void checkPaymentNow();
    void stopWaitingForPayment();
    void dismissPurchase(const QString &purchaseId);

    void refreshServers();
    // Prepares a webpack server for CoCo's installer (sudo setup, pinned host key) and hands the
    // credentials to InstallController; configReady() then opens the protocol wizard.
    void configureServer(const QString &serverPublicId, const QString &password = QString());

signals:
    void accountChanged();
    void busyChanged();
    void walletChanged();
    void plansChanged();
    void paymentChanged();
    void purchasesChanged();
    void serversChanged();

    void signInFinished(bool ok);
    void errorOccurred(const QString &message);
    void notificationShown(const QString &message);
    void orderPlaced();
    void configReady();
    void passwordNeeded(const QString &serverPublicId, const QString &serverName);

private:
    void placeOrder(const QString &planPublicId, const QString &regionPublicId, const QString &operatingSystemId);
    void reportFailure(const webpack::ApiResult &result);
    QString messageFor(const webpack::ApiResult &result) const;
    QString formatMoney(double amount, const QString &currencyCode) const;
    const webpack::Plan *findPlan(const QString &planPublicId) const;
    void setBusy(bool on);
    void setPaymentText(const QString &text);

    webpack::Integration *m_integration;
    InstallUiController *m_installUiController;
    QList<webpack::Plan> m_plans;
    webpack::Wallet m_wallet;
    int m_busy = 0;
    QString m_paymentText;

    // The order to place once the balance covers it.
    QString m_waitPlan;
    QString m_waitRegion;
    QString m_waitSystem;
};

#endif // WEBPACKUICONTROLLER_H
