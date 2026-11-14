#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

#include "core/controllers/webpack/webpackApiClient.h"
#include "core/controllers/webpack/webpackDeviceKey.h"
#include "core/controllers/webpack/webpackOrderAttempt.h"
#include "core/controllers/webpack/webpackPurchases.h"
#include "core/controllers/webpack/webpackShop.h"
#include "core/controllers/webpack/webpackStore.h"
#include "fakeNetwork.h"

using namespace webpack;

namespace
{
    // JSON fixtures with single quotes, to keep them readable.
    QByteArray j(const char *text)
    {
        QByteArray json(text);
        json.replace('\'', '"');
        return json;
    }

    struct FixedTokens : TokenSource
    {
        QString accessToken() const override { return QStringLiteral("wpa_T"); }
        void refreshAccessToken(const std::function<void(bool)> &done) override { done(false); }
    };

    ApiResult refusal(int status, const QString &code, ApiOutcome outcome = ApiOutcome::Rejected)
    {
        ApiResult r;
        r.httpStatus = status;
        r.errorCode = code;
        r.outcome = outcome;
        return r;
    }

    QByteArray keysListWith(const QString &fingerprint)
    {
        return FakeNetwork::ok("[{\"keyPublicId\":\"key_1\",\"fingerprint\":\"SHA256:" + fingerprint.toLatin1() + "\"}]");
    }
} // namespace

class TestWebpackShop : public QObject
{
    Q_OBJECT

private slots:
    void shortfallRounding()
    {
        QCOMPARE(shortfall(150000, 200000, "IRT"), 0.0);
        QCOMPARE(shortfall(150000, 149001, "IRT"), 1000.0);
        QCOMPARE(shortfall(150000, 10000, "IRT"), 140000.0);
        QCOMPARE(shortfall(150500, 0, "IRT"), 151000.0);
        QCOMPARE(shortfall(4.99, 1.00, "USD"), 3.99);
        QCOMPARE(shortfall(4.99, 4.985, "EUR"), 0.01);
        QCOMPARE(shortfall(4.99, 4.99, "EUR"), 0.0);
    }

    void moneyAndGateways()
    {
        QCOMPARE(moneyString(150000, "IRT"), QStringLiteral("150000"));
        QCOMPARE(moneyString(4.5, "USD"), QStringLiteral("4.50"));
        QCOMPARE(gatewaysFor("IRT"), QStringList { "zarinpal" });
        QVERIFY(gatewaysFor("USD").contains("stripe"));
        QVERIFY(!gatewaysFor("EUR").contains("zarinpal"));
    }

    void parsesPlansAndFiltersSystems()
    {
        const QByteArray json = j("{'items':[{'planPublicId':'plan_1','displayName':'Small','specifications':'1 vCPU'," "'monthlyPriceUsd':5,'price':{'amount':350000,'currencyCode':'IRT'}," "'billingTerms':{'periodMonths':1,'graceDays':3,'deleteAfterPowerOffDays':7,'refund':'none'}," "'regions':[{'regionPublicId':'reg_de','displayName':'Germany','city':'Frankfurt','countryCode':'DE'}]," "'operatingSystems':[{'operatingSystemId':'os_win','displayName':'Windows','isDefault':false}," "{'operatingSystemId':'os_deb','displayName':'Debian 12','isDefault':false}," "{'operatingSystemId':'os_u24','displayName':'Ubuntu 24.04','isDefault':true}]}," "{'planPublicId':'plan_2','displayName':'No price','specifications':'','monthlyPriceUsd':null,'price':null}]," "'page':1,'pageSize':100,'totalCount':2}");
        const QList<Plan> plans = parsePlans(QJsonDocument::fromJson(json).object());
        QCOMPARE(plans.size(), 2);
        QVERIFY(plans.at(0).hasPrice);
        QCOMPARE(plans.at(0).priceAmount, 350000.0);
        QCOMPARE(plans.at(0).regions.at(0).city, QStringLiteral("Frankfurt"));
        QCOMPARE(plans.at(0).graceDays, 3);
        QVERIFY(!plans.at(1).hasPrice);
        const QList<PlanSystem> systems = supportedSystems(plans.at(0).systems);
        QCOMPARE(systems.size(), 2);
        QCOMPARE(systems.at(0).operatingSystemId, QStringLiteral("os_u24"));
    }

    void orderAttemptKeepsKeyUntilDefiniteAnswer()
    {
        OrderAttempt attempt;
        const QString first = attempt.keyFor("plan_1", "reg", "", "100.00", "IRT");
        QCOMPARE(attempt.keyFor("plan_1", "reg", "", "100.00", "IRT"), first);
        QVERIFY(attempt.keyFor("plan_2", "reg", "", "100.00", "IRT") != first); // no answer yet, choice changed

        OrderAttempt kept;
        const QString key = kept.keyFor("plan_1", "reg", "", "100.00", "IRT");
        kept.after(refusal(0, "unreachable", ApiOutcome::Unreachable));
        QVERIFY(kept.isPending());
        QCOMPARE(kept.keyFor("plan_2", "reg2", "", "9.00", "USD"), key); // same key whatever is chosen
        kept.after(refusal(409, "order_pending_review"));
        QVERIFY(kept.isPending());
        kept.after(refusal(502, "http_502", ApiOutcome::ServerError));
        QVERIFY(kept.isPending());
        kept.after(refusal(402, "insufficient_balance"));
        QVERIFY(!kept.isPending());
        QVERIFY(kept.keyFor("plan_1", "reg", "", "100.00", "IRT") != key);
    }

    void chargeFreeCodes_data()
    {
        QTest::addColumn<QString>("code");
        QTest::addColumn<bool>("chargeFree");
        for (const char *c : { "insufficient_balance", "price_changed", "not_found", "not_available", "invalid_request",
                               "order_failed", "previous_failed", "idempotency_conflict" }) {
            QTest::newRow(c) << QString::fromLatin1(c) << true;
        }
        for (const char *c : { "order_pending_review", "rate_limited", "internal_error", "something_new" }) {
            QTest::newRow(c) << QString::fromLatin1(c) << false;
        }
    }

    void chargeFreeCodes()
    {
        QFETCH(QString, code);
        QFETCH(bool, chargeFree);
        QCOMPARE(OrderAttempt::nothingCharged(refusal(400, code)), chargeFree);
    }

    void loginRules()
    {
        for (int i = 0; i < 200; ++i) {
            const QString pw = generateLoginPassword();
            QCOMPARE(pw.size(), 24);
            bool letter = false, digit = false, other = false;
            for (const QChar c : pw) {
                letter |= c.isLetter() && c.unicode() < 128;
                digit |= c.isDigit();
                other |= !c.isLetterOrNumber() || c.unicode() >= 128;
            }
            QVERIFY(letter && digit && !other);
        }
        QVERIFY(generateLoginPassword() != generateLoginPassword());
        QVERIFY(isValidLoginUsername("coco"));
        QVERIFY(!isValidLoginUsername("root"));
        QVERIFY(!isValidLoginUsername("Co"));
        QVERIFY(!isValidLoginUsername("co co"));
        QVERIFY(!isValidLoginUsername("1coco"));
        QVERIFY(!isValidLoginUsername("coco;rm"));
    }

    void accessReadiness()
    {
        QJsonObject access { { "installState", "installed" }, { "clientKeyFingerprint", "SHA256:abc" }, { "hostKeySha256", "hk" } };
        QVERIFY(accessIsReady(access, "abc"));
        QVERIFY(!accessIsReady(access, "other"));
        access["hostKeySha256"] = QJsonValue::Null;
        QVERIFY(!accessIsReady(access, "abc"));
        access["hostKeySha256"] = "hk";
        access["installState"] = "pending";
        QVERIFY(!accessIsReady(access, "abc"));
    }

    void purchaseHappyPath()
    {
        FakeNetwork network;
        MemoryStore store;
        ApiClient api(&network);
        FixedTokens tokens;
        api.setTokenSource(&tokens);
        DeviceKey key(&api, &store);
        Purchases purchases(&api, &key, &store);
        purchases.setPollIntervals(20, 20);
        QSignalSpy ready(&purchases, &Purchases::serverReady);
        const QString fp = key.keyPair().fingerprint;

        network.queue("/vps/ssh-keys", 200, keysListWith(fp));
        network.queue("/vps/orders", 200,
                      FakeNetwork::ok(j("{'orderPublicId':'order_1','serverPublicId':'server_1','status':'processing','chargedAmount':350000,'currencyCode':'IRT'}")));
        network.queue("/vps/orders/order_1", 200, FakeNetwork::ok(j("{'orderPublicId':'order_1','serverPublicId':'server_1','status':'processing'}")));
        network.queue("/vps/orders/order_1", 200, FakeNetwork::ok(j("{'orderPublicId':'order_1','serverPublicId':'server_1','status':'succeeded'}")));
        network.queue("/vps/servers/server_1/access", 200, FakeNetwork::ok(j("{'loginUsername':'coco','installState':'pending'}")));
        network.queue("/vps/servers/server_1/access", 200,
                      FakeNetwork::ok("{\"loginUsername\":\"coco\",\"installState\":\"installed\",\"clientKeyFingerprint\":\"SHA256:" + fp.toLatin1()
                                      + "\",\"hostKeySha256\":\"HOSTKEY\"}"));

        ApiResult placed;
        bool done = false;
        purchases.placeOrder("plan_1", "Small", "reg_de", "Germany", "", 350000, "IRT", [&](const ApiResult &r, const Purchase &) {
            placed = r;
            done = true;
        });
        QTRY_VERIFY(done);
        QVERIFY(placed.isOk());

        // The order carried the device key, the CoCo login, and the price the person saw.
        QByteArray orderBody;
        for (const auto &s : network.seen) {
            if (s.path.endsWith("/vps/orders")) {
                orderBody = s.body;
            }
        }
        const QJsonObject sent = QJsonDocument::fromJson(orderBody).object();
        QCOMPARE(sent.value("sshKeyPublicId").toString(), QStringLiteral("key_1"));
        QCOMPARE(sent.value("loginUsername").toString(), QStringLiteral("coco"));
        QCOMPARE(sent.value("loginPassword").toString().size(), 24);
        QCOMPARE(sent.value("expectedAmount").toDouble(), 350000.0);
        QCOMPARE(sent.value("expectedCurrencyCode").toString(), QStringLiteral("IRT"));
        QVERIFY(!sent.value("idempotencyKey").toString().isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
        QCOMPARE(purchases.purchases().at(0).stage, Purchase::Stage::Ready);
        const ServerFacts facts = purchases.facts("server_1");
        QCOMPARE(facts.hostKeySha256, QStringLiteral("HOSTKEY"));
        QCOMPARE(facts.loginPassword, sent.value("loginPassword").toString());
        QVERIFY(purchases.purchases().at(0).loginPassword.isEmpty());

        // Facts survive a restart.
        Purchases restarted(&api, &key, &store);
        QCOMPARE(restarted.facts("server_1").hostKeySha256, QStringLiteral("HOSTKEY"));
    }

    void unclearOrderIsResentWithTheSameKeyAndPassword()
    {
        FakeNetwork network;
        MemoryStore store;
        ApiClient api(&network);
        FixedTokens tokens;
        api.setTokenSource(&tokens);
        DeviceKey key(&api, &store);
        const QString fp = key.keyPair().fingerprint;

        QJsonObject firstSent;
        {
            Purchases purchases(&api, &key, &store);
            network.queue("/vps/ssh-keys", 200, keysListWith(fp));
            network.queue("/vps/orders", 0, {});
            bool done = false;
            purchases.placeOrder("plan_1", "Small", "reg_de", "Germany", "", 350000, "IRT", [&](const ApiResult &, const Purchase &) { done = true; });
            QTRY_VERIFY(done);
            QVERIFY(purchases.hasPendingOrder());
            QCOMPARE(purchases.purchases().size(), 1);
            firstSent = QJsonDocument::fromJson(network.seen.last().body).object();
        }

        // After a restart, a press with another choice still sends the first order unchanged.
        Purchases purchases(&api, &key, &store);
        QVERIFY(purchases.hasPendingOrder());
        network.queue("/vps/ssh-keys", 200, keysListWith(fp));
        network.queue("/vps/orders", 200, FakeNetwork::ok(j("{'orderPublicId':'order_1','serverPublicId':'server_1','status':'processing'}")));
        bool done = false;
        purchases.placeOrder("plan_2", "Big", "reg_fi", "Finland", "", 900000, "IRT", [&](const ApiResult &, const Purchase &) { done = true; });
        QTRY_VERIFY(done);
        const QJsonObject secondSent = QJsonDocument::fromJson(network.seen.last().body).object();
        QCOMPARE(secondSent.value("idempotencyKey"), firstSent.value("idempotencyKey"));
        QCOMPARE(secondSent.value("loginPassword"), firstSent.value("loginPassword"));
        QCOMPARE(secondSent.value("planPublicId").toString(), QStringLiteral("plan_1"));
        QVERIFY(!purchases.hasPendingOrder());
    }

    void chargeFreeRefusalLeavesNothingBehind()
    {
        FakeNetwork network;
        MemoryStore store;
        ApiClient api(&network);
        FixedTokens tokens;
        api.setTokenSource(&tokens);
        DeviceKey key(&api, &store);
        Purchases purchases(&api, &key, &store);
        network.queue("/vps/ssh-keys", 200, keysListWith(key.keyPair().fingerprint));
        network.queue("/vps/orders", 402, FakeNetwork::fail("insufficient_balance"));
        ApiResult got;
        bool done = false;
        purchases.placeOrder("plan_1", "Small", "reg", "DE", "", 10, "USD", [&](const ApiResult &r, const Purchase &) {
            got = r;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(got.errorCode, QStringLiteral("insufficient_balance"));
        QVERIFY(purchases.purchases().isEmpty());
        QVERIFY(!purchases.hasPendingOrder());
    }

    void pendingReviewFollowsTheExistingOrder()
    {
        FakeNetwork network;
        MemoryStore store;
        ApiClient api(&network);
        FixedTokens tokens;
        api.setTokenSource(&tokens);
        DeviceKey key(&api, &store);
        Purchases purchases(&api, &key, &store);
        purchases.setPollIntervals(1000000, 1000000);
        network.queue("/vps/ssh-keys", 200, keysListWith(key.keyPair().fingerprint));
        network.queue("/vps/orders", 409, FakeNetwork::fail("order_pending_review", "Being checked. ExistingOrder:order_77"));
        bool done = false;
        purchases.placeOrder("plan_1", "Small", "reg", "DE", "", 10, "USD", [&](const ApiResult &, const Purchase &) { done = true; });
        QTRY_VERIFY(done);
        QCOMPARE(purchases.purchases().at(0).orderPublicId, QStringLiteral("order_77"));
        QCOMPARE(purchases.purchases().at(0).stage, Purchase::Stage::Building);
        QVERIFY(purchases.hasPendingOrder());
    }

    void checkoutKeyIsKeptAfterUnclearAnswer()
    {
        FakeNetwork network;
        ApiClient api(&network);
        FixedTokens tokens;
        api.setTokenSource(&tokens);
        Shop shop(&api);

        network.queue("/wallet/payment-checkouts", 0, {});
        bool done = false;
        shop.startCheckout(140000, "IRT", "zarinpal", [&](const ApiResult &, const Checkout &) { done = true; });
        QTRY_VERIFY(done);
        const QJsonObject first = QJsonDocument::fromJson(network.seen.last().body).object();
        QCOMPARE(first.value("amount").toDouble(), 140000.0);
        QCOMPARE(first.value("gatewayCode").toString(), QStringLiteral("zarinpal"));

        network.queue("/wallet/payment-checkouts", 200,
                      FakeNetwork::ok(j("{'paymentIntentPublicId':'pi_1','amount':140000,'currencyCode':'IRT','checkoutUrl':'https://pay.example/x','expiresAtUtc':'2030-01-01T00:00:00Z'}")));
        Checkout checkout;
        done = false;
        shop.startCheckout(141000, "IRT", "zarinpal", [&](const ApiResult &, const Checkout &c) {
            checkout = c;
            done = true;
        });
        QTRY_VERIFY(done);
        const QJsonObject second = QJsonDocument::fromJson(network.seen.last().body).object();
        QCOMPARE(second.value("idempotencyKey"), first.value("idempotencyKey"));
        QCOMPARE(checkout.checkoutUrl.toString(), QStringLiteral("https://pay.example/x"));
    }

    void checkoutRefusesNonHttpsLink()
    {
        FakeNetwork network;
        ApiClient api(&network);
        FixedTokens tokens;
        api.setTokenSource(&tokens);
        Shop shop(&api);
        network.queue("/wallet/payment-checkouts", 200,
                      FakeNetwork::ok(j("{'paymentIntentPublicId':'pi_1','amount':1,'currencyCode':'USD','checkoutUrl':'http://pay.example/x'}")));
        ApiResult got;
        bool done = false;
        shop.startCheckout(1, "USD", "stripe", [&](const ApiResult &r, const Checkout &) {
            got = r;
            done = true;
        });
        QTRY_VERIFY(done);
        QVERIFY(!got.isOk());
    }

    void balanceWatchStopsWhenEnough()
    {
        FakeNetwork network;
        ApiClient api(&network);
        FixedTokens tokens;
        api.setTokenSource(&tokens);
        Shop shop(&api);
        QSignalSpy reached(&shop, &Shop::balanceReached);
        network.queue("/wallet/balance", 200, FakeNetwork::ok(j("{'currentBalance':1000,'currencyCode':'IRT'}")));
        network.queue("/wallet/balance", 200, FakeNetwork::ok(j("{'currentBalance':350000,'currencyCode':'IRT'}")));
        shop.watchBalance(350000, QDateTime::currentDateTimeUtc().addSecs(600));
        shop.checkBalanceNow();
        QTRY_VERIFY_WITH_TIMEOUT(!network.seen.isEmpty(), 2000);
        shop.checkBalanceNow();
        QTRY_COMPARE_WITH_TIMEOUT(reached.count(), 1, 8000);
        QVERIFY(!shop.isWatching());
    }
};

QTEST_MAIN(TestWebpackShop)
#include "testWebpackShop.moc"
