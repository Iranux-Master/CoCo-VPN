#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

#include "core/utils/constants/protocolConstants.h"
#include "core/utils/dnstt/dnsttUtils.h"

using namespace amnezia;

// CoCo DNS tunnel (docs/plans/D1-dns-tunnel.md): domain rules, resolver parsing and the client-config rewrite.
class TestDnsttUtils : public QObject
{
    Q_OBJECT

private slots:
    void normalizesDomains()
    {
        QCOMPARE(dnstt::normalizeTunnelDomain(" T.Example.COM. "), QStringLiteral("t.example.com"));
        QCOMPARE(dnstt::normalizeTunnelDomain("https://t.example.com/x"), QStringLiteral("t.example.com"));
    }

    void validatesDomains_data()
    {
        QTest::addColumn<QString>("domain");
        QTest::addColumn<bool>("valid");
        QTest::newRow("subdomain") << "t.example.com" << true;
        QTest::newRow("deeper") << "t.vpn.example.co.uk" << true;
        QTest::newRow("hyphen") << "my-tunnel.example.com" << true;
        QTest::newRow("apex is refused") << "example.com" << false;
        QTest::newRow("single label") << "localhost" << false;
        QTest::newRow("ip address") << "192.0.2.1" << false;
        QTest::newRow("leading hyphen") << "-t.example.com" << false;
        QTest::newRow("trailing hyphen") << "t-.example.com" << false;
        QTest::newRow("underscore") << "t_1.example.com" << false;
        QTest::newRow("shell characters") << "t.example.com;reboot" << false;
        QTest::newRow("space") << "t .example.com" << false;
        QTest::newRow("upper case is not normalized here") << "T.example.com" << false;
        QTest::newRow("empty label") << "t..example.com" << false;
        QTest::newRow("label too long for -ns") << QString(61, 'a') + ".example.com" << false;
        QTest::newRow("empty") << "" << false;
    }

    void validatesDomains()
    {
        QFETCH(QString, domain);
        QFETCH(bool, valid);
        QCOMPARE(dnstt::isValidTunnelDomain(domain), valid);
    }

    void buildsRecords()
    {
        QCOMPARE(dnstt::nameServerHost("t.example.com"), QStringLiteral("t-ns.example.com"));
        QCOMPARE(dnstt::nameServerHost("example.com"), QString());
        const QStringList records = dnstt::requiredRecords("t.example.com", "203.0.113.7");
        QCOMPARE(records, (QStringList { "NS  t.example.com.  ->  t-ns.example.com.", "A  t-ns.example.com.  ->  203.0.113.7" }));
    }

    void normalizesMtuAndKeys()
    {
        QCOMPARE(dnstt::normalizeMtu(""), QStringLiteral("1232"));
        QCOMPARE(dnstt::normalizeMtu("900"), QStringLiteral("900"));
        QCOMPARE(dnstt::normalizeMtu("100"), QStringLiteral("1232"));
        QCOMPARE(dnstt::normalizeMtu("9000"), QStringLiteral("1232"));
        QCOMPARE(dnstt::normalizeMtu("12a"), QStringLiteral("1232"));
        QVERIFY(dnstt::isValidPublicKey(QString(64, 'a')));
        QVERIFY(!dnstt::isValidPublicKey(QString(63, 'a')));
        QVERIFY(!dnstt::isValidPublicKey(QString(64, 'A')));
    }

    void parsesResolvers()
    {
        auto r = dnstt::parseResolver("udp:192.0.2.1");
        QVERIFY(r.isValid());
        QCOMPARE(r.address, QStringLiteral("192.0.2.1:53"));
        QCOMPARE(r.ip, QStringLiteral("192.0.2.1"));

        r = dnstt::parseResolver("udp:192.0.2.1:5353");
        QCOMPARE(r.address, QStringLiteral("192.0.2.1:5353"));

        r = dnstt::parseResolver("udp:2001:db8::1");
        QVERIFY(r.isValid());
        QCOMPARE(r.address, QStringLiteral("[2001:db8::1]:53"));

        r = dnstt::parseResolver("dot:[2001:db8::1]:853");
        QCOMPARE(r.transport, dnstt::Resolver::Transport::Dot);
        QCOMPARE(r.ip, QStringLiteral("2001:db8::1"));

        r = dnstt::parseResolver("doh:https://1.1.1.1/dns-query");
        QCOMPARE(r.transport, dnstt::Resolver::Transport::Doh);
        QCOMPARE(r.ip, QStringLiteral("1.1.1.1"));
        QCOMPARE(dnstt::resolverToSpec(r), QStringLiteral("doh:https://1.1.1.1/dns-query"));

        // Names are refused: the device must route the exact address around the tunnel.
        QVERIFY(!dnstt::parseResolver("doh:https://dns.google/dns-query").isValid());
        QVERIFY(!dnstt::parseResolver("udp:dns.example.com").isValid());
        QVERIFY(!dnstt::parseResolver("doh:http://1.1.1.1/dns-query").isValid());
        QVERIFY(!dnstt::parseResolver("tcp:1.1.1.1").isValid());
        QVERIFY(!dnstt::parseResolver("udp:1.1.1.1:99999").isValid());
    }

    void ordersDefaultResolvers()
    {
        const auto list = dnstt::defaultResolvers({ "192.168.1.1", "bogus" });
        QCOMPARE(list.size(), 3);
        QCOMPARE(dnstt::resolverToSpec(list.at(0)), QStringLiteral("udp:192.168.1.1:53"));
        QCOMPARE(list.at(1).transport, dnstt::Resolver::Transport::Doh);
    }

    void buildsClientArguments()
    {
        const auto r = dnstt::parseResolver("udp:192.0.2.1");
        const QString key(64, 'b');
        QCOMPARE(dnstt::clientArguments(r, key, "t.example.com", 41000),
                 (QStringList { "-udp", "192.0.2.1:53", "-pubkey", key, "t.example.com", "127.0.0.1:41000" }));
    }

    void rewritesXrayConfigForLocalPort()
    {
        const QJsonObject client = QJsonDocument::fromJson(R"({
            "inbounds": [{"listen": "127.0.0.1", "port": 10808, "protocol": "socks"}],
            "outbounds": [{"protocol": "vless", "mux": {"enabled": true},
                           "settings": {"vnext": [{"address": "127.0.0.1", "port": 8053,
                                                    "users": [{"id": "u", "encryption": "none"}]}]},
                           "streamSettings": {"network": "tcp", "security": "none"}}],
            "dnstt": {"domain": "t.example.com", "pubkey": "k"}
        })").object();

        const QJsonObject xray = dnstt::xrayConfigForLocalPort(client, 41000);
        QVERIFY(!xray.contains(QLatin1String(protocols::dnstt::block)));
        const QJsonObject server = xray["outbounds"].toArray().at(0).toObject()["settings"].toObject()["vnext"].toArray().at(0).toObject();
        QCOMPARE(server["address"].toString(), QStringLiteral("127.0.0.1"));
        QCOMPARE(server["port"].toInt(), 41000);
        QCOMPARE(server["users"].toArray().at(0).toObject()["id"].toString(), QStringLiteral("u"));
        QVERIFY(xray["outbounds"].toArray().at(0).toObject()["mux"].toObject()["enabled"].toBool());

        QVERIFY(dnstt::xrayConfigForLocalPort(QJsonObject { { "outbounds", QJsonArray {} } }, 1).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDnsttUtils)
#include "testDnsttUtils.moc"
