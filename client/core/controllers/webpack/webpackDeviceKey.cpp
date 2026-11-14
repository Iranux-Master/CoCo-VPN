#include "webpackDeviceKey.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>

#include <libssh/libssh.h>

#include "core/utils/selfhosted/sshHostKey.h"
#include "webpackApiClient.h"
#include "webpackStore.h"

namespace webpack
{
    namespace
    {
        KeyPair describe(ssh_key privateKey, const QString &comment)
        {
            KeyPair pair;
            char *privateB64 = nullptr;
            if (ssh_pki_export_privkey_base64(privateKey, nullptr, nullptr, nullptr, &privateB64) != SSH_OK || !privateB64) {
                return {};
            }
            pair.privateKey = QString::fromLatin1(privateB64);
            ssh_string_free_char(privateB64);

            ssh_key publicKey = nullptr;
            if (ssh_pki_export_privkey_to_pubkey(privateKey, &publicKey) != SSH_OK || !publicKey) {
                return {};
            }
            char *publicB64 = nullptr;
            const int rc = ssh_pki_export_pubkey_base64(publicKey, &publicB64);
            const char *type = ssh_key_type_to_char(ssh_key_type(publicKey));
            ssh_key_free(publicKey);
            if (rc != SSH_OK || !publicB64) {
                return {};
            }
            const QByteArray blob = QByteArray::fromBase64(QByteArray(publicB64));
            pair.publicKeyLine = QStringLiteral("%1 %2").arg(QString::fromLatin1(type), QString::fromLatin1(publicB64));
            if (!comment.isEmpty()) {
                pair.publicKeyLine += QLatin1Char(' ') + comment;
            }
            ssh_string_free_char(publicB64);
            pair.fingerprint = sshHostKey::fingerprintOfBlob(blob);
            return pair;
        }
    } // namespace

    KeyPair generateEd25519KeyPair(const QString &comment)
    {
        ssh_key key = nullptr;
        if (ssh_pki_generate(SSH_KEYTYPE_ED25519, 0, &key) != SSH_OK || !key) {
            return {};
        }
        KeyPair pair = describe(key, comment);
        ssh_key_free(key);
        return pair;
    }

    KeyPair keyPairFromPrivateKey(const QString &privateKey, const QString &comment)
    {
        ssh_key key = nullptr;
        const QByteArray pem = privateKey.toLatin1();
        if (ssh_pki_import_privkey_base64(pem.constData(), nullptr, nullptr, nullptr, &key) != SSH_OK || !key) {
            return {};
        }
        KeyPair pair = describe(key, comment);
        ssh_key_free(key);
        return pair;
    }

    DeviceKey::DeviceKey(ApiClient *api, Store *store, QObject *parent) : QObject(parent), m_api(api), m_store(store)
    {
    }

    KeyPair DeviceKey::keyPair()
    {
        if (m_cached.isValid()) {
            return m_cached;
        }
        const QString comment = QStringLiteral("CoCo VPN");
        const QString stored = m_store->value(keys::deviceKey).toString();
        if (!stored.isEmpty()) {
            m_cached = keyPairFromPrivateKey(stored, comment);
        }
        if (!m_cached.isValid()) {
            m_cached = generateEd25519KeyPair(comment);
            if (m_cached.isValid()) {
                m_store->setValue(keys::deviceKey, m_cached.privateKey);
            }
        }
        return m_cached;
    }

    QString DeviceKey::findKeyId(const QJsonValue &listData, const QString &fingerprint)
    {
        const QString wanted = sshHostKey::normalizeFingerprint(fingerprint);
        const QJsonArray items = listData.isArray() ? listData.toArray() : listData.toObject().value("items").toArray();
        for (const QJsonValue &item : items) {
            const QJsonObject key = item.toObject();
            if (sshHostKey::normalizeFingerprint(key.value("fingerprint").toString()) == wanted) {
                return key.value("keyPublicId").toString();
            }
        }
        return {};
    }

    void DeviceKey::ensureRegistered(const QString &keyName, const Registered &done)
    {
        listThenAdd(keyName, true, done);
    }

    void DeviceKey::listThenAdd(const QString &keyName, bool addWhenMissing, const Registered &done)
    {
        const KeyPair pair = keyPair();
        if (!pair.isValid()) {
            ApiResult failure;
            failure.outcome = ApiOutcome::Rejected;
            failure.errorCode = QStringLiteral("device_key_unavailable");
            done(failure, {});
            return;
        }

        QPointer<DeviceKey> self(this);
        m_api->get(QStringLiteral("api/v1/vps/ssh-keys"), [self, pair, keyName, addWhenMissing, done](const ApiResult &listed) {
            if (!self) {
                return;
            }
            if (!listed.isOk()) {
                done(listed, {});
                return;
            }
            const QString id = findKeyId(listed.data, pair.fingerprint);
            if (!id.isEmpty() || !addWhenMissing) {
                done(listed, id);
                return;
            }
            const QJsonObject body { { "name", keyName.left(64) }, { "publicKey", pair.publicKeyLine } };
            self->m_api->post(QStringLiteral("api/v1/vps/ssh-keys"), body, [self, keyName, done](const ApiResult &added) {
                if (!self) {
                    return;
                }
                if (added.isOk()) {
                    done(added, added.object().value("keyPublicId").toString());
                } else if (added.errorCode == QLatin1String("duplicate_key")) {
                    // Added meanwhile (another CoCo window, or the website): read the list again.
                    self->listThenAdd(keyName, false, done);
                } else {
                    done(added, {});
                }
            });
        });
    }
} // namespace webpack
