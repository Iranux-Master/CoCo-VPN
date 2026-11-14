#include "webpackUiController.h"

#include <QDesktopServices>
#include <QFile>
#include <QFutureWatcher>
#include <QLocale>
#include <QtConcurrent>

#include "core/controllers/webpack/webpackApiClient.h"
#include "core/controllers/webpack/webpackIntegration.h"
#include "core/controllers/webpack/webpackOrderAttempt.h"
#include "core/controllers/webpack/webpackPurchases.h"
#include "core/controllers/webpack/webpackServers.h"
#include "core/controllers/webpack/webpackSession.h"
#include "core/controllers/webpack/webpackSudoSetup.h"
#include "ui/controllers/selfhosted/installUiController.h"

using namespace webpack;

WebpackUiController::WebpackUiController(Integration *integration, InstallUiController *installUiController, QObject *parent)
    : QObject(parent), m_integration(integration), m_installUiController(installUiController)
{
    connect(m_integration->session(), &Session::stateChanged, this, &WebpackUiController::accountChanged);
    connect(m_integration->session(), &Session::profileChanged, this, &WebpackUiController::accountChanged);
    connect(m_integration->purchases(), &Purchases::changed, this, &WebpackUiController::purchasesChanged);
    connect(m_integration->servers(), &Servers::changed, this, &WebpackUiController::serversChanged);

    connect(m_integration->purchases(), &Purchases::serverReady, this, [this](const QString &, const QString &planName) {
        emit notificationShown(tr("Your server \"%1\" is ready. Open \"Servers bought from webpack\" and press Config.").arg(planName));
        refreshServers();
    });

    Shop *shop = m_integration->shop();
    connect(shop, &Shop::balanceChanged, this, [this](double balance, const QString &currency) {
        m_wallet.balance = balance;
        m_wallet.currencyCode = currency;
        emit walletChanged();
    });
    connect(shop, &Shop::balanceReached, this, [this]() {
        setPaymentText(tr("Payment received. Placing your order..."));
        emit paymentChanged();
        placeOrder(m_waitPlan, m_waitRegion, m_waitSystem);
    });
    connect(shop, &Shop::watchExpired, this, [this]() {
        setPaymentText(QString());
        emit paymentChanged();
        emit errorOccurred(tr("CoCo stopped waiting for the payment. If you paid, the money is in your webpack wallet: press Buy again."));
    });
}

bool WebpackUiController::signedIn() const
{
    return m_integration->session()->isSignedIn();
}

bool WebpackUiController::offline() const
{
    return m_integration->session()->state() == Session::State::Offline;
}

QString WebpackUiController::accountEmail() const
{
    return m_integration->session()->profile().email;
}

QString WebpackUiController::walletText() const
{
    if (m_wallet.currencyCode.isEmpty()) {
        return QString();
    }
    return formatMoney(m_wallet.balance, m_wallet.currencyCode);
}

QStringList WebpackUiController::gateways() const
{
    QStringList names;
    for (const QString &code : gatewaysFor(m_wallet.currencyCode)) {
        if (code == QLatin1String("zarinpal")) names << tr("ZarinPal (Iranian bank cards)");
        else if (code == QLatin1String("stripe")) names << tr("Card (Stripe)");
        else if (code == QLatin1String("paypal")) names << QStringLiteral("PayPal");
        else if (code == QLatin1String("nowpayments")) names << tr("Cryptocurrency (NOWPayments)");
        else names << code;
    }
    return names;
}

bool WebpackUiController::waitingForPayment() const
{
    return m_integration->shop()->isWatching();
}

bool WebpackUiController::hasPendingOrder() const
{
    return m_integration->purchases()->hasPendingOrder();
}

QString WebpackUiController::formatMoney(double amount, const QString &currencyCode) const
{
    QLocale locale;
    if (currencyCode.compare(QLatin1String("IRT"), Qt::CaseInsensitive) == 0) {
        return tr("%1 Toman").arg(locale.toString(qint64(std::llround(amount))));
    }
    return QStringLiteral("%1 %2").arg(locale.toString(amount, 'f', 2), currencyCode);
}

QVariantList WebpackUiController::plans() const
{
    QVariantList list;
    for (const Plan &plan : m_plans) {
        QVariantList regions;
        for (const PlanRegion &region : plan.regions) {
            regions.append(QVariantMap { { "id", region.regionPublicId },
                                         { "name", region.city.isEmpty() ? region.displayName
                                                                         : region.city + QStringLiteral(" · ") + region.displayName } });
        }
        QVariantList systems;
        for (const PlanSystem &system : supportedSystems(plan.systems)) {
            systems.append(QVariantMap { { "id", system.isDefault ? QString() : system.operatingSystemId }, { "name", system.displayName } });
        }
        if (plan.systems.isEmpty()) {
            systems.append(QVariantMap { { "id", QString() }, { "name", tr("Default system") } });
        }
        const bool sellable = plan.hasPrice && !regions.isEmpty() && !systems.isEmpty();
        list.append(QVariantMap { { "id", plan.planPublicId },
                                  { "name", plan.displayName },
                                  { "specifications", plan.specifications },
                                  { "priceText", plan.hasPrice ? tr("%1 per month").arg(formatMoney(plan.priceAmount, plan.priceCurrency))
                                                               : tr("Not sold in your wallet currency") },
                                  { "termsText", tr("Renews monthly from your wallet. If unpaid, the server is powered off after %1 days and deleted %2 days later.")
                                                         .arg(plan.graceDays)
                                                         .arg(plan.deleteAfterPowerOffDays) },
                                  { "regions", regions },
                                  { "systems", systems },
                                  { "sellable", sellable } });
    }
    return list;
}

QVariantList WebpackUiController::purchases() const
{
    QVariantList list;
    for (const Purchase &p : m_integration->purchases()->purchases()) {
        QString stageText;
        switch (p.stage) {
        case Purchase::Stage::Ordering:
            stageText = p.errorCode.isEmpty() ? tr("Placing the order...")
                                              : tr("The answer was unclear. Press Buy again: the same order is sent, you are not charged twice.");
            break;
        case Purchase::Stage::Building:
            stageText = p.errorCode == QLatin1String("order_pending_review")
                    ? tr("webpack is checking this order. Nothing more will be charged.")
                    : tr("Building the server...");
            break;
        case Purchase::Stage::InstallingAccess: stageText = tr("Installing this device's access..."); break;
        case Purchase::Stage::Ready: stageText = tr("Ready to configure"); break;
        case Purchase::Stage::Failed:
            stageText = p.errorCode == QLatin1String("access_failed")
                    ? tr("webpack could not set up access. Contact webpack support.")
                    : tr("The order failed and was refunded.");
            break;
        }
        list.append(QVariantMap { { "id", p.id },
                                  { "planName", p.planName },
                                  { "regionName", p.regionName },
                                  { "stage", Purchase::stageName(p.stage) },
                                  { "stageText", stageText },
                                  { "serverPublicId", p.serverPublicId },
                                  { "dismissable", p.stage == Purchase::Stage::Ready || p.stage == Purchase::Stage::Failed } });
    }
    return list;
}

QVariantList WebpackUiController::servers() const
{
    QVariantList list;
    for (const AccountServer &s : m_integration->servers()->servers()) {
        QString status;
        if (s.state.compare(QLatin1String("OnHold"), Qt::CaseInsensitive) == 0) {
            status = tr("On hold: contact webpack support");
        } else if (s.accessReady) {
            status = tr("Ready to configure");
        } else if (s.installState == QLatin1String("pending")) {
            status = tr("Installing access...");
        } else if (s.installState == QLatin1String("installed") && s.hostKeySha256.isEmpty()) {
            status = tr("webpack is still confirming the server's identity");
        } else if (s.installState == QLatin1String("failed")) {
            status = tr("Access setup failed: contact webpack support");
        } else {
            status = tr("This device has no access yet: press Config to add it");
        }
        list.append(QVariantMap { { "id", s.serverPublicId },
                                  { "name", s.name },
                                  { "description", QStringList { s.ipAddress, s.location, s.profile }.filter(QRegularExpression(".+")).join(QStringLiteral(" · ")) },
                                  { "status", status },
                                  { "ready", s.accessReady },
                                  { "passwordKnown", s.passwordKnown } });
    }
    return list;
}

void WebpackUiController::setBusy(bool on)
{
    m_busy = qMax(0, m_busy + (on ? 1 : -1));
    emit busyChanged();
}

void WebpackUiController::setPaymentText(const QString &text)
{
    m_paymentText = text;
}

void WebpackUiController::signIn(const QString &email, const QString &password)
{
    if (email.trimmed().isEmpty() || password.isEmpty()) {
        emit errorOccurred(tr("Enter your webpack email and password."));
        return;
    }
    setBusy(true);
    m_integration->session()->signInWithPassword(email, password, [this](const ApiResult &result) {
        setBusy(false);
        if (!result.isOk()) {
            reportFailure(result);
        }
        emit signInFinished(result.isOk());
        emit accountChanged();
    });
}

void WebpackUiController::signInWithGoogle()
{
    setBusy(true);
    emit notificationShown(tr("Continue the sign-in in your browser, then come back to CoCo VPN."));
    m_integration->session()->signInWithGoogle([this](const ApiResult &result) {
        setBusy(false);
        if (!result.isOk() && result.errorCode != QLatin1String("cancelled")) {
            reportFailure(result);
        }
        emit signInFinished(result.isOk());
        emit accountChanged();
    });
}

void WebpackUiController::cancelGoogleSignIn()
{
    m_integration->session()->cancelGoogleSignIn();
}

void WebpackUiController::signOut()
{
    m_integration->shop()->stopWatching();
    m_integration->session()->signOut([this]() {
        m_plans.clear();
        m_wallet = {};
        emit plansChanged();
        emit walletChanged();
        emit accountChanged();
    });
}

void WebpackUiController::openAccountOnWebsite()
{
    QDesktopServices::openUrl(m_integration->api()->urlFor(QStringLiteral("servers")));
}

void WebpackUiController::loadShop()
{
    if (!signedIn()) {
        return;
    }
    setBusy(true);
    m_integration->shop()->loadPlans([this](const ApiResult &result, const QList<Plan> &plans) {
        setBusy(false);
        if (!result.isOk()) {
            reportFailure(result);
            return;
        }
        m_plans = plans;
        emit plansChanged();
    });
    setBusy(true);
    m_integration->shop()->loadWallet([this](const ApiResult &result, const Wallet &wallet) {
        setBusy(false);
        if (result.isOk()) {
            m_wallet = wallet;
            emit walletChanged();
        }
    });
}

const Plan *WebpackUiController::findPlan(const QString &planPublicId) const
{
    for (const Plan &plan : m_plans) {
        if (plan.planPublicId == planPublicId) {
            return &plan;
        }
    }
    return nullptr;
}

void WebpackUiController::buy(const QString &planPublicId, const QString &regionPublicId, const QString &operatingSystemId, int gatewayIndex)
{
    const Plan *plan = findPlan(planPublicId);
    if (!plan || !plan->hasPrice || regionPublicId.isEmpty()) {
        emit errorOccurred(tr("Choose a plan and a location first."));
        return;
    }
    // An unclear earlier order is always sent again first, whatever is chosen now.
    if (hasPendingOrder()) {
        placeOrder(planPublicId, regionPublicId, operatingSystemId);
        return;
    }

    const double price = plan->priceAmount;
    const QString currency = plan->priceCurrency;
    setBusy(true);
    m_integration->shop()->loadWallet([this, planPublicId, regionPublicId, operatingSystemId, gatewayIndex, price, currency](const ApiResult &walletResult, const Wallet &wallet) {
        setBusy(false);
        if (!walletResult.isOk()) {
            reportFailure(walletResult);
            return;
        }
        m_wallet = wallet;
        emit walletChanged();
        if (wallet.currencyCode != currency) {
            emit errorOccurred(tr("The price changed. Review the new price and try again."));
            loadShop();
            return;
        }

        const double missing = shortfall(price, wallet.balance, currency);
        if (missing <= 0) {
            placeOrder(planPublicId, regionPublicId, operatingSystemId);
            return;
        }

        const QStringList codes = gatewaysFor(currency);
        const QString gateway = codes.value(qBound(0, gatewayIndex, int(codes.size()) - 1));
        setBusy(true);
        m_integration->shop()->startCheckout(missing, currency, gateway, [this, planPublicId, regionPublicId, operatingSystemId, price, missing, currency](const ApiResult &result, const Checkout &checkout) {
            setBusy(false);
            if (!result.isOk()) {
                reportFailure(result);
                return;
            }
            if (!QDesktopServices::openUrl(checkout.checkoutUrl)) {
                emit errorOccurred(tr("CoCo could not open your browser for the payment."));
                return;
            }
            m_waitPlan = planPublicId;
            m_waitRegion = regionPublicId;
            m_waitSystem = operatingSystemId;
            setPaymentText(tr("Pay %1 in your browser. CoCo places the order as soon as the money reaches your wallet.")
                                   .arg(formatMoney(missing, currency)));
            m_integration->shop()->watchBalance(price, checkout.expiresAtUtc);
            emit paymentChanged();
        });
    });
}

void WebpackUiController::checkPaymentNow()
{
    m_integration->shop()->checkBalanceNow();
}

void WebpackUiController::stopWaitingForPayment()
{
    // Never cancels the payment at webpack: a payer who already paid must not cancel.
    m_integration->shop()->stopWatching();
    setPaymentText(QString());
    emit paymentChanged();
}

void WebpackUiController::placeOrder(const QString &planPublicId, const QString &regionPublicId, const QString &operatingSystemId)
{
    const Plan *plan = findPlan(planPublicId);
    if (!plan) {
        emit errorOccurred(tr("Choose a plan and a location first."));
        return;
    }
    QString regionName;
    for (const PlanRegion &region : plan->regions) {
        if (region.regionPublicId == regionPublicId) {
            regionName = region.city.isEmpty() ? region.displayName : region.city;
        }
    }
    setBusy(true);
    m_integration->purchases()->placeOrder(plan->planPublicId, plan->displayName, regionPublicId, regionName, operatingSystemId,
                                           plan->priceAmount, plan->priceCurrency, [this](const ApiResult &result, const Purchase &) {
        setBusy(false);
        setPaymentText(QString());
        emit paymentChanged();
        emit purchasesChanged();
        if (result.isOk() || result.errorCode == QLatin1String("order_pending_review")) {
            emit orderPlaced();
            emit notificationShown(tr("Order placed. CoCo is following the build; it usually takes a few minutes."));
        } else {
            reportFailure(result);
            if (OrderAttempt::reloadsPlans(result)) {
                loadShop();
            }
        }
    });
}

void WebpackUiController::dismissPurchase(const QString &purchaseId)
{
    m_integration->purchases()->dismiss(purchaseId);
}

void WebpackUiController::refreshServers()
{
    if (!signedIn()) {
        return;
    }
    setBusy(true);
    m_integration->servers()->refresh([this](const ApiResult &result) {
        setBusy(false);
        if (!result.isOk()) {
            reportFailure(result);
        }
    });
}

void WebpackUiController::configureServer(const QString &serverPublicId, const QString &password)
{
    Servers *servers = m_integration->servers();
    const AccountServer *server = servers->find(serverPublicId);
    if (!server) {
        return;
    }
    if (!server->accessReady) {
        if (server->installState == QLatin1String("installed") && server->hostKeySha256.isEmpty()) {
            emit errorOccurred(tr("webpack is still confirming this server's identity. Try again in a few minutes."));
            return;
        }
        if (server->installState == QLatin1String("pending")) {
            emit notificationShown(tr("webpack is installing this device's access. Try again in a few minutes."));
            return;
        }
        setBusy(true);
        servers->requestDeviceAccess(serverPublicId, [this](const ApiResult &result) {
            setBusy(false);
            if (result.isOk()) {
                emit notificationShown(tr("This device's key is being added to the server. Press Config again in a few minutes."));
                refreshServers();
            } else {
                reportFailure(result);
            }
        });
        return;
    }

    const amnezia::ServerCredentials credentials = servers->credentialsFor(serverPublicId);
    if (!credentials.isValid()) {
        emit errorOccurred(tr("The server's details are not complete yet. Refresh the list and try again."));
        return;
    }

    ServerFacts facts = m_integration->purchases()->facts(serverPublicId);
    const QString sudoPassword = password.isEmpty() ? facts.loginPassword : password;
    const QString serverName = server->name;

    QFile scriptFile(QStringLiteral(":/server_scripts/webpack_grant_sudo.sh"));
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Internal error"));
        return;
    }
    const QString script = QString::fromUtf8(scriptFile.readAll()).remove(QLatin1Char('\r'));

    setBusy(true);
    auto *watcher = new QFutureWatcher<SudoSetupResult>(this);
    connect(watcher, &QFutureWatcher<SudoSetupResult>::finished, this,
            [this, watcher, credentials, serverPublicId, serverName, facts, passwordGiven = !password.isEmpty()]() mutable {
                watcher->deleteLater();
                setBusy(false);
                const SudoSetupResult result = watcher->result();
                switch (result.outcome) {
                case SudoSetupResult::Outcome::Ready:
                case SudoSetupResult::Outcome::AlreadyReady:
                    facts.serverPublicId = serverPublicId;
                    facts.loginUsername = credentials.userName;
                    facts.hostKeySha256 = credentials.hostKeySha256;
                    facts.sudoReady = true;
                    facts.loginPassword.clear(); // not needed any more; webpack can still show it
                    m_integration->purchases()->saveFacts(facts);
                    m_installUiController->setProcessedServerCredentials(credentials);
                    emit configReady();
                    break;
                case SudoSetupResult::Outcome::WrongPassword:
                    if (passwordGiven) {
                        emit errorOccurred(tr("That password was not accepted by the server."));
                    }
                    emit passwordNeeded(serverPublicId, serverName);
                    break;
                case SudoSetupResult::Outcome::HostKeyMismatch:
                    emit errorOccurred(tr("The server's identity (SSH host key) does not match the one webpack.ir confirmed. The connection was stopped to protect you. Contact webpack support."));
                    break;
                case SudoSetupResult::Outcome::AuthFailed:
                    emit errorOccurred(tr("The server refused this device's key. Press Config again after a few minutes, or contact webpack support."));
                    break;
                case SudoSetupResult::Outcome::Unreachable:
                    emit errorOccurred(tr("The server cannot be reached. Check that it is running and try again."));
                    break;
                case SudoSetupResult::Outcome::ScriptFailed:
                    emit errorOccurred(tr("CoCo could not prepare administrator access on the server."));
                    break;
                }
            });
    watcher->setFuture(QtConcurrent::run([credentials, sudoPassword, script]() {
        return setUpPasswordlessSudo(credentials, sudoPassword, script);
    }));
}

void WebpackUiController::reportFailure(const ApiResult &result)
{
    if (result.outcome == ApiOutcome::SignedOut) {
        emit accountChanged();
    }
    emit errorOccurred(messageFor(result));
}

QString WebpackUiController::messageFor(const ApiResult &result) const
{
    const QString code = result.errorCode;
    QString text;
    if (result.outcome == ApiOutcome::Unreachable) text = tr("webpack.ir cannot be reached. Check your internet connection and try again.");
    else if (result.outcome == ApiOutcome::RateLimited) text = tr("Too many requests. Wait a minute and try again.");
    else if (result.outcome == ApiOutcome::SignedOut) text = tr("Your webpack sign-in has ended. Sign in again.");
    else if (result.outcome == ApiOutcome::ServerError) text = tr("webpack.ir had a problem. Try again later.");
    else if (code == QLatin1String("invalid_credentials")) text = tr("The email or password is not correct.");
    else if (code == QLatin1String("two_factor_required")) text = tr("This account uses two-step sign-in. Sign in with Google, or on the website.");
    else if (code == QLatin1String("password_change_required")) text = tr("Change your password on the webpack website first.");
    else if (code == QLatin1String("email_not_confirmed")) text = tr("Confirm your email address first (see the email from webpack).");
    else if (code == QLatin1String("account_not_approved")) text = tr("This webpack account is not active yet.");
    else if (code == QLatin1String("google_unavailable")) text = tr("Google sign-in is not available right now.");
    else if (code == QLatin1String("google_timed_out")) text = tr("The Google sign-in took too long. Try again.");
    else if (code == QLatin1String("google_failed") || code == QLatin1String("invalid_token")) text = tr("The Google sign-in did not finish. Try again.");
    else if (code == QLatin1String("browser_unavailable")) text = tr("CoCo could not open your browser.");
    else if (code == QLatin1String("insufficient_balance")) text = tr("Your wallet balance is not enough for this server.");
    else if (code == QLatin1String("price_changed")) text = tr("The price changed. Review the new price and try again.");
    else if (code == QLatin1String("not_found") || code == QLatin1String("not_available")) text = tr("This plan or location is not available now. Choose another one.");
    else if (code == QLatin1String("order_failed") || code == QLatin1String("previous_failed")) text = tr("The order failed and the money was returned to your wallet.");
    else if (code == QLatin1String("invalid_request")) text = tr("webpack refused the order details. Nothing was charged.");
    else if (code == QLatin1String("idempotency_conflict")) text = tr("An earlier order with other details is still open. Check your servers on the webpack website.");
    else if (code == QLatin1String("device_key_unavailable")) text = tr("CoCo could not create this device's SSH key.");
    else if (code == QLatin1String("too_many_keys")) text = tr("Your webpack account has too many SSH keys. Remove one on the website.");
    else if (code.startsWith(QLatin1String("payment_"))) text = tr("The payment could not be started (%1). Try another gateway or top up on the website.").arg(code);
    else if (!result.message.isEmpty()) text = result.message;
    else text = tr("Something went wrong (%1).").arg(code);

    if (!result.correlationId.isEmpty() && result.outcome != ApiOutcome::Ok) {
        text += QLatin1Char(' ') + tr("Support code: %1").arg(result.correlationId);
    }
    return text;
}
