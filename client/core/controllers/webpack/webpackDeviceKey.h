#ifndef WEBPACKDEVICEKEY_H
#define WEBPACKDEVICEKEY_H

#include <QObject>
#include <QString>

#include <functional>

#include "webpackApiResult.h"

namespace webpack
{
    class ApiClient;
    class Store;

    struct KeyPair
    {
        QString privateKey;    // OpenSSH PEM, kept only in the Store (encrypted)
        QString publicKeyLine; // "ssh-ed25519 AAAA... CoCo VPN"
        QString fingerprint;   // OpenSSH SHA-256, no prefix, no padding
        bool isValid() const { return !privateKey.isEmpty() && !publicKeyLine.isEmpty() && !fingerprint.isEmpty(); }
    };

    // libssh helpers (no network).
    KeyPair generateEd25519KeyPair(const QString &comment);
    KeyPair keyPairFromPrivateKey(const QString &privateKey, const QString &comment);

    // This device's SSH key for servers bought from webpack: one Ed25519 key per Windows profile,
    // created once, registered on the account (POST /vps/ssh-keys) when it is not there yet. Only the
    // public half ever leaves the device.
    class DeviceKey : public QObject
    {
        Q_OBJECT

    public:
        using Registered = std::function<void(const ApiResult &result, const QString &sshKeyPublicId)>;

        DeviceKey(ApiClient *api, Store *store, QObject *parent = nullptr);

        // Loads the key, or creates and stores one.
        KeyPair keyPair();
        // The account's id for this key, adding it when missing.
        void ensureRegistered(const QString &keyName, const Registered &done);

        // Pure: the id of the key with this fingerprint in a GET /vps/ssh-keys answer, or empty.
        static QString findKeyId(const QJsonValue &listData, const QString &fingerprint);

    private:
        void listThenAdd(const QString &keyName, bool addWhenMissing, const Registered &done);

        ApiClient *m_api;
        Store *m_store;
        KeyPair m_cached;
    };
} // namespace webpack

#endif // WEBPACKDEVICEKEY_H
