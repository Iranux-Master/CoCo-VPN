#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "core/controllers/webpack/webpackApiClient.h"
#include "core/controllers/webpack/webpackDeviceKey.h"
#include "core/controllers/webpack/webpackStore.h"
#include "core/utils/selfhosted/sshHostKey.h"
#include "fakeNetwork.h"

using namespace webpack;

class TestWebpackKeys : public QObject
{
    Q_OBJECT

    struct NoTokens : TokenSource
    {
        QString accessToken() const override { return QStringLiteral("wpa_T"); }
        void refreshAccessToken(const std::function<void(bool)> &done) override { done(false); }
    };

private slots:
    void normalizesFingerprints()
    {
        QCOMPARE(sshHostKey::normalizeFingerprint(" SHA256:abc== "), QStringLiteral("abc"));
        QCOMPARE(sshHostKey::normalizeFingerprint("abc"), QStringLiteral("abc"));
    }

    void generatesAndReimportsEd25519()
    {
        const KeyPair pair = generateEd25519KeyPair("CoCo VPN");
        QVERIFY(pair.isValid());
        QVERIFY(pair.privateKey.contains("BEGIN OPENSSH PRIVATE KEY"));
        QVERIFY(pair.publicKeyLine.startsWith("ssh-ed25519 AAAA"));
        QVERIFY(pair.publicKeyLine.endsWith(" CoCo VPN"));
        QCOMPARE(pair.fingerprint.size(), 43);

        const KeyPair again = keyPairFromPrivateKey(pair.privateKey, "CoCo VPN");
        QCOMPARE(again.fingerprint, pair.fingerprint);
        QCOMPARE(again.publicKeyLine, pair.publicKeyLine);
    }

    void fingerprintIsOpenSshSha256()
    {
        // OpenSSH: base64(SHA-256(blob)) without padding; value computed independently with Python's hashlib.
        const QByteArray blob = QByteArray::fromBase64("AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl");
        QCOMPARE(sshHostKey::fingerprintOfBlob(blob), QStringLiteral("+DiY3wvvV6TuJJhbpZisF/zLDA0zPMSvHdkr4UvCOqU"));
    }

    void fingerprintAgreesWithSshKeygen()
    {
        const QString keygen = QStandardPaths::findExecutable("ssh-keygen");
        if (keygen.isEmpty()) {
            QSKIP("ssh-keygen not installed");
        }
        const KeyPair pair = generateEd25519KeyPair("CoCo VPN");
        QTemporaryDir dir;
        QFile file(dir.filePath("key.pub"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(pair.publicKeyLine.toLatin1() + "\n");
        file.close();
        QProcess process;
        process.start(keygen, { "-l", "-E", "sha256", "-f", file.fileName() });
        QVERIFY(process.waitForFinished(10000));
        const QString out = QString::fromLatin1(process.readAllStandardOutput());
        QVERIFY2(out.contains("SHA256:" + pair.fingerprint), qPrintable(out));
    }

    void deviceKeyIsStoredOnce()
    {
        MemoryStore store;
        FakeNetwork network;
        ApiClient api(&network);
        DeviceKey key(&api, &store);
        const KeyPair first = key.keyPair();
        QVERIFY(first.isValid());
        QCOMPARE(store.value(keys::deviceKey).toString(), first.privateKey);

        DeviceKey reopened(&api, &store);
        QCOMPARE(reopened.keyPair().fingerprint, first.fingerprint);
    }

    void findsKeyByFingerprint()
    {
        const QJsonValue list = QJsonDocument::fromJson(
                "[{\"keyPublicId\":\"key_1\",\"fingerprint\":\"SHA256:other\"},{\"keyPublicId\":\"key_2\",\"fingerprint\":\"SHA256:mine\"}]").array();
        QCOMPARE(DeviceKey::findKeyId(list, "mine"), QStringLiteral("key_2"));
        QVERIFY(DeviceKey::findKeyId(list, "none").isEmpty());
    }

    void registersWhenMissingAndReusesWhenPresent()
    {
        MemoryStore store;
        FakeNetwork network;
        ApiClient api(&network);
        NoTokens tokens;
        api.setTokenSource(&tokens);
        DeviceKey key(&api, &store);
        const KeyPair pair = key.keyPair();

        network.queue("/vps/ssh-keys", 200, FakeNetwork::ok("[]"));
        network.queue("/vps/ssh-keys", 200, FakeNetwork::ok("{\"keyPublicId\":\"key_new\",\"fingerprint\":\"SHA256:" + pair.fingerprint.toLatin1() + "\"}"));
        QString id;
        bool done = false;
        key.ensureRegistered("CoCo VPN · test", [&](const ApiResult &, const QString &keyId) {
            id = keyId;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(id, QStringLiteral("key_new"));
        QCOMPARE(network.seen.size(), 2);
        QVERIFY(network.seen.at(1).body.contains("ssh-ed25519"));
        QVERIFY(!network.seen.at(1).body.contains("PRIVATE"));

        network.queue("/vps/ssh-keys", 200, FakeNetwork::ok("[{\"keyPublicId\":\"key_new\",\"fingerprint\":\"SHA256:" + pair.fingerprint.toLatin1() + "\"}]"));
        done = false;
        key.ensureRegistered("CoCo VPN · test", [&](const ApiResult &, const QString &keyId) {
            id = keyId;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(id, QStringLiteral("key_new"));
        QCOMPARE(network.seen.size(), 3);
    }
};

QTEST_MAIN(TestWebpackKeys)
#include "testWebpackKeys.moc"
