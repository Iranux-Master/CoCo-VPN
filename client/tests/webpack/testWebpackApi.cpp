#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>

#include "core/controllers/webpack/webpackApiClient.h"
#include "core/controllers/webpack/webpackLoopback.h"
#include "core/controllers/webpack/webpackSession.h"
#include "core/controllers/webpack/webpackStore.h"
#include "fakeNetwork.h"

using namespace webpack;

namespace
{
    QByteArray tokens(const QByteArray &access, const QByteArray &refresh)
    {
        const QByteArray expires = QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs).toLatin1();
        const QByteArray refreshExpires = QDateTime::currentDateTimeUtc().addDays(30).toString(Qt::ISODateWithMs).toLatin1();
        return FakeNetwork::ok("{\"sessionId\":\"ds_1\",\"accessToken\":\"" + access + "\",\"accessTokenExpiresAtUtc\":\"" + expires
                               + "\",\"refreshToken\":\"" + refresh + "\",\"refreshTokenExpiresAtUtc\":\"" + refreshExpires + "\"}");
    }

    ApiResult waitFor(const std::function<void(const ApiClient::Callback &)> &call)
    {
        ApiResult got;
        bool done = false;
        call([&](const ApiResult &r) {
            got = r;
            done = true;
        });
        (void)QTest::qWaitFor([&]() { return done; }, 3000);
        return got;
    }
} // namespace

class TestWebpackApi : public QObject
{
    Q_OBJECT

private slots:
    void envelopeSuccess()
    {
        const ApiResult r = parseApiResult(200, FakeNetwork::ok("{\"a\":1}"));
        QVERIFY(r.isOk());
        QCOMPARE(r.object().value("a").toInt(), 1);
        QCOMPARE(r.correlationId, QStringLiteral("srv-1"));
    }

    void envelopeRefusal()
    {
        const ApiResult r = parseApiResult(402, FakeNetwork::fail("insufficient_balance"));
        QCOMPARE(r.outcome, ApiOutcome::Rejected);
        QCOMPARE(r.errorCode, QStringLiteral("insufficient_balance"));
        QCOMPARE(r.httpStatus, 402);
    }

    void envelopeValidationErrors()
    {
        const ApiResult r = parseApiResult(
                400, "{\"success\":false,\"error\":{\"code\":\"invalid_request\",\"message\":\"m\",\"validationErrors\":{\"email\":[\"bad\"]}}}");
        QCOMPARE(r.errorCode, QStringLiteral("invalid_request"));
        QVERIFY(r.validationErrors.contains("email"));
    }

    void nonJsonAnswers()
    {
        QCOMPARE(parseApiResult(502, "<html>bad gateway</html>").outcome, ApiOutcome::ServerError);
        QCOMPARE(parseApiResult(401, "").outcome, ApiOutcome::SignedOut);
        const ApiResult limited = parseApiResult(429, "", "17");
        QCOMPARE(limited.outcome, ApiOutcome::RateLimited);
        QCOMPARE(limited.retryAfterSeconds, 17);
        QCOMPARE(parseApiResult(200, "not json").outcome, ApiOutcome::BadResponse);
        QCOMPARE(parseApiResult(0, {}, {}, "x").outcome, ApiOutcome::Unreachable);
    }

    void successFalseWith200IsBad()
    {
        QCOMPARE(parseApiResult(200, FakeNetwork::fail("x")).outcome, ApiOutcome::BadResponse);
    }

    void correlationIds()
    {
        const QString id = newCorrelationId();
        QVERIFY(isValidCorrelationId(id));
        QVERIFY(id.size() <= 64);
        QVERIFY(newCorrelationId() != id);
        QVERIFY(!isValidCorrelationId("has space"));
        QVERIFY(!isValidCorrelationId(QString(65, 'a')));
    }

    void baseUrlRules()
    {
        QVERIFY(isAllowedBaseUrl(QUrl("https://webpack.ir/"), false));
        QVERIFY(!isAllowedBaseUrl(QUrl("http://webpack.ir/"), false));
        QVERIFY(!isAllowedBaseUrl(QUrl("http://webpack.ir/"), true));
        QVERIFY(isAllowedBaseUrl(QUrl("http://127.0.0.1:5000/"), true));
        QVERIFY(isAllowedBaseUrl(QUrl("http://localhost:5000/"), true));
        QVERIFY(!isAllowedBaseUrl(QUrl("http://127.0.0.1:5000/"), false));
        QVERIFY(!isAllowedBaseUrl(QUrl("https://user:pw@webpack.ir/"), false));
        QVERIFY(!isAllowedBaseUrl(QUrl("ftp://webpack.ir/"), false));
        QCOMPARE(normalisedBaseUrl(QUrl("https://webpack.ir/sub")).toString(), QStringLiteral("https://webpack.ir/sub/"));
    }

    void clientSendsHeadersAndPath()
    {
        FakeNetwork network;
        ApiClient api(&network);
        network.queue("/api/v1/health", 200, FakeNetwork::ok("{}"));
        const ApiResult r = waitFor([&](const ApiClient::Callback &cb) { api.get("api/v1/health", cb, { false, 1000 }); });
        QVERIFY(r.isOk());
        QCOMPARE(network.seen.size(), 1);
        QCOMPARE(network.seen.at(0).path, QStringLiteral("/api/v1/health"));
        QVERIFY(network.seen.at(0).authorization.isEmpty());
        QVERIFY(isValidCorrelationId(QString::fromLatin1(network.seen.at(0).correlationId)));
    }

    void clientWithoutAccountIsSignedOut()
    {
        FakeNetwork network;
        ApiClient api(&network);
        const ApiResult r = waitFor([&](const ApiClient::Callback &cb) { api.get("api/v1/vps/plans", cb); });
        QCOMPARE(r.outcome, ApiOutcome::SignedOut);
        QCOMPARE(network.seen.size(), 0);
    }

    void transportFailure()
    {
        FakeNetwork network;
        ApiClient api(&network);
        network.queue("/x", 0, {});
        const ApiResult r = waitFor([&](const ApiClient::Callback &cb) { api.get("x", cb, { false, 1000 }); });
        QCOMPARE(r.outcome, ApiOutcome::Unreachable);
    }

    void passwordSignInStoresOnlyRefreshToken()
    {
        FakeNetwork network;
        MemoryStore store;
        ApiClient api(&network);
        Session session(&api, &store);
        api.setTokenSource(&session);

        network.queue("/auth/device-sessions", 200, tokens("wpa_A", "wpr_A"));
        network.queue("/auth/device-sessions/current", 200,
                      FakeNetwork::ok("{\"email\":\"a@b.c\",\"displayName\":\"A\",\"walletCurrency\":\"IRT\"}"));

        ApiResult got;
        bool done = false;
        session.signInWithPassword("a@b.c", "secret-pw", [&](const ApiResult &r) {
            got = r;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
        QVERIFY(got.isOk());
        QCOMPARE(session.state(), Session::State::SignedIn);
        QCOMPARE(session.profile().walletCurrency, QStringLiteral("IRT"));
        QCOMPARE(session.accessToken(), QStringLiteral("wpa_A"));
        // The profile call carried the bearer.
        QCOMPARE(network.seen.last().authorization, QByteArray("Bearer wpa_A"));

        const QByteArray stored = store.value(keys::session).toByteArray();
        QVERIFY(stored.contains("wpr_A"));
        QVERIFY(!stored.contains("wpa_A"));
        QVERIFY(!stored.contains("secret-pw"));

        // A new session object (an app restart) is signed in from the store, without an access token.
        Session restarted(&api, &store);
        QCOMPARE(restarted.state(), Session::State::SignedIn);
        QVERIFY(restarted.accessToken().isEmpty());
    }

    void refreshesOnceOn401AndRetries()
    {
        FakeNetwork network;
        MemoryStore store;
        ApiClient api(&network);
        Session session(&api, &store);
        api.setTokenSource(&session);

        network.queue("/auth/device-sessions", 200, tokens("wpa_A", "wpr_A"));
        network.queue("/current", 200, FakeNetwork::ok("{}"));
        bool signedIn = false;
        session.signInWithPassword("a@b.c", "pw", [&](const ApiResult &) { signedIn = true; });
        QTRY_VERIFY(signedIn);

        network.queue("/vps/plans", 401, FakeNetwork::fail("invalid_token"));
        network.queue("/auth/device-sessions/refresh", 200, tokens("wpa_B", "wpr_B"));
        network.queue("/vps/plans", 200, FakeNetwork::ok("{\"items\":[]}"));
        const ApiResult r = waitFor([&](const ApiClient::Callback &cb) { api.get("api/v1/vps/plans", cb); });
        QVERIFY(r.isOk());
        QCOMPARE(network.seen.last().authorization, QByteArray("Bearer wpa_B"));
        QVERIFY(network.seen.at(network.seen.size() - 2).body.contains("wpr_A"));
        QVERIFY(store.value(keys::session).toByteArray().contains("wpr_B"));
    }

    void concurrentRefreshesShareOneRequest()
    {
        FakeNetwork network;
        MemoryStore store;
        store.setValue(keys::session, QByteArray("{\"sessionId\":\"ds_1\",\"refreshToken\":\"wpr_A\"}"));
        ApiClient api(&network);
        Session session(&api, &store);
        api.setTokenSource(&session);

        network.queue("/refresh", 200, tokens("wpa_B", "wpr_B"));
        network.queue("/a", 200, FakeNetwork::ok("{}"));
        network.queue("/b", 200, FakeNetwork::ok("{}"));
        int finished = 0;
        api.get("api/v1/a", [&](const ApiResult &r) { finished += r.isOk(); });
        api.get("api/v1/b", [&](const ApiResult &r) { finished += r.isOk(); });
        QTRY_COMPARE_WITH_TIMEOUT(finished, 2, 3000);
        int refreshes = 0;
        for (const auto &s : network.seen) {
            refreshes += s.path.endsWith("/refresh");
        }
        QCOMPARE(refreshes, 1);
    }

    void refusedRefreshSignsOut()
    {
        FakeNetwork network;
        MemoryStore store;
        store.setValue(keys::session, QByteArray("{\"sessionId\":\"ds_1\",\"refreshToken\":\"wpr_A\"}"));
        ApiClient api(&network);
        Session session(&api, &store);
        api.setTokenSource(&session);

        network.queue("/refresh", 401, FakeNetwork::fail("invalid_token"));
        const ApiResult r = waitFor([&](const ApiClient::Callback &cb) { api.get("api/v1/vps/servers", cb); });
        QCOMPARE(r.outcome, ApiOutcome::SignedOut);
        QCOMPARE(session.state(), Session::State::SignedOut);
        QVERIFY(!store.contains(keys::session));
    }

    void unreachableRefreshKeepsSignIn()
    {
        FakeNetwork network;
        MemoryStore store;
        store.setValue(keys::session, QByteArray("{\"sessionId\":\"ds_1\",\"refreshToken\":\"wpr_A\"}"));
        ApiClient api(&network);
        Session session(&api, &store);
        api.setTokenSource(&session);

        network.queue("/refresh", 0, {});
        waitFor([&](const ApiClient::Callback &cb) { api.get("api/v1/vps/servers", cb); });
        QCOMPARE(session.state(), Session::State::Offline);
        QVERIFY(store.contains(keys::session));
    }

    void pkceMatchesRfc7636Example()
    {
        // RFC 7636 appendix B.
        QCOMPARE(Pkce::challengeFor("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
                 QStringLiteral("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
        const Pkce pkce = Pkce::create();
        QCOMPARE(pkce.verifier.size(), 43);
        QCOMPARE(pkce.challenge, Pkce::challengeFor(pkce.verifier));
    }

    void loopbackParser()
    {
        auto q = parseLoopbackRequest("GET /webpack-signin?request=dsr_1&code=a%2Bb+c HTTP/1.1\r\nHost: x\r\n\r\n");
        QVERIFY(q.has_value());
        QCOMPARE(q->value("request"), QStringLiteral("dsr_1"));
        QCOMPARE(q->value("code"), QStringLiteral("a+b c"));
        QVERIFY(!parseLoopbackRequest("POST /webpack-signin?code=1 HTTP/1.1\r\n").has_value());
        QVERIFY(!parseLoopbackRequest("GET /other?code=1 HTTP/1.1\r\n").has_value());
        QVERIFY(!parseLoopbackRequest("garbage").has_value());
    }

    void loopbackReceiverAnswersAndEmits()
    {
        LoopbackReceiver receiver;
        QVERIFY(receiver.start());
        QVERIFY(receiver.port() > 0);
        QSignalSpy spy(&receiver, &LoopbackReceiver::returned);

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, receiver.port());
        QVERIFY(socket.waitForConnected(2000));
        socket.write("GET /webpack-signin?request=dsr_9&code=xyz HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        const auto query = spy.at(0).at(0).value<QMap<QString, QString>>();
        QCOMPARE(query.value("code"), QStringLiteral("xyz"));
        QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() > 0 || socket.state() != QAbstractSocket::ConnectedState, 3000);
        QVERIFY(socket.readAll().startsWith("HTTP/1.1 200 OK"));
    }
};

QTEST_MAIN(TestWebpackApi)
#include "testWebpackApi.moc"
