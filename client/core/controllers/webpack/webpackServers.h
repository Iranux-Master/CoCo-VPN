#ifndef WEBPACKSERVERS_H
#define WEBPACKSERVERS_H

#include <QList>
#include <QObject>

#include <functional>

#include "core/utils/commonStructs.h"
#include "webpackApiResult.h"

namespace webpack
{
    class ApiClient;
    class DeviceKey;
    class Purchases;

    struct AccountServer
    {
        QString serverPublicId;
        QString name;
        QString state;
        QString ipAddress;
        QString location;
        QString image;
        QString profile;
        int sshPort = 22;
        QString loginUsername;
        QString installState;
        QString clientKeyFingerprint;
        QString hostKeySha256;
        bool accessReady = false;   // installed with this device's key and a pinned host key
        bool passwordKnown = false; // CoCo bought it (or sudo is already set up)
    };

    // The account's servers (GET /vps/servers, their detail and /access), and the credentials
    // CoCo's installer needs for one of them.
    class Servers : public QObject
    {
        Q_OBJECT

    public:
        using Done = std::function<void(const ApiResult &result)>;

        Servers(ApiClient *api, DeviceKey *deviceKey, Purchases *purchases, QObject *parent = nullptr);

        QList<AccountServer> servers() const { return m_servers; }
        const AccountServer *find(const QString &serverPublicId) const;

        void refresh(const Done &done = {});
        // Installs this device's key on a server bought elsewhere (website, another device).
        void requestDeviceAccess(const QString &serverPublicId, const Done &done);
        // Device key, login, address and pinned host key. Invalid until access is ready.
        amnezia::ServerCredentials credentialsFor(const QString &serverPublicId) const;

    signals:
        void changed();

    private:
        void loadDetails(int generation);

        ApiClient *m_api;
        DeviceKey *m_deviceKey;
        Purchases *m_purchases;
        QList<AccountServer> m_servers;
        int m_generation = 0;
        int m_pending = 0;
        Done m_refreshDone;
    };
} // namespace webpack

#endif // WEBPACKSERVERS_H
