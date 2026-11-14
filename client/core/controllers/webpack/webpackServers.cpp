#include "webpackServers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QUrl>

#include "core/utils/selfhosted/sshHostKey.h"
#include "webpackApiClient.h"
#include "webpackDeviceKey.h"
#include "webpackPurchases.h"
#include "webpackSession.h"

namespace webpack
{
    namespace
    {
        constexpr int maxServers = 30;

        ApiResult okResult()
        {
            ApiResult result;
            result.outcome = ApiOutcome::Ok;
            result.httpStatus = 200;
            return result;
        }

        QString serverPath(const QString &id, const QString &suffix = {})
        {
            return QStringLiteral("api/v1/vps/servers/") + QString::fromLatin1(QUrl::toPercentEncoding(id)) + suffix;
        }
    } // namespace

    Servers::Servers(ApiClient *api, DeviceKey *deviceKey, Purchases *purchases, QObject *parent)
        : QObject(parent), m_api(api), m_deviceKey(deviceKey), m_purchases(purchases)
    {
    }

    const AccountServer *Servers::find(const QString &serverPublicId) const
    {
        for (const AccountServer &server : m_servers) {
            if (server.serverPublicId == serverPublicId) {
                return &server;
            }
        }
        return nullptr;
    }

    void Servers::refresh(const Done &done)
    {
        const int generation = ++m_generation;
        m_refreshDone = done;
        QPointer<Servers> self(this);
        m_api->get(QStringLiteral("api/v1/vps/servers?page=1&pageSize=%1").arg(maxServers), [self, generation](const ApiResult &result) {
            if (!self || generation != self->m_generation) {
                return;
            }
            if (!result.isOk()) {
                if (self->m_refreshDone) {
                    self->m_refreshDone(result);
                }
                return;
            }
            QList<AccountServer> listed;
            const QJsonValue data = result.data;
            const QJsonArray items = data.isArray() ? data.toArray() : data.toObject().value("items").toArray();
            for (const QJsonValue &value : items) {
                const QJsonObject item = value.toObject();
                AccountServer server;
                server.serverPublicId = item.value("serverPublicId").toString();
                server.name = item.value("name").toString();
                server.state = item.value("state").toString();
                if (!server.serverPublicId.isEmpty()) {
                    listed.append(server);
                }
            }
            self->m_servers = listed;
            emit self->changed();
            self->loadDetails(generation);
        });
    }

    void Servers::loadDetails(int generation)
    {
        m_pending = m_servers.size() * 2;
        if (m_pending == 0) {
            if (m_refreshDone) {
                m_refreshDone(okResult());
            }
            return;
        }
        const QString fingerprint = m_deviceKey->keyPair().fingerprint;
        QPointer<Servers> self(this);
        auto finishOne = [self, generation]() {
            if (!self || generation != self->m_generation) {
                return;
            }
            if (--self->m_pending == 0) {
                emit self->changed();
                if (self->m_refreshDone) {
                    self->m_refreshDone(okResult());
                }
            }
        };

        for (const AccountServer &server : m_servers) {
            const QString id = server.serverPublicId;
            m_api->get(serverPath(id), [self, generation, id, finishOne](const ApiResult &result) {
                if (self && generation == self->m_generation && result.isOk()) {
                    for (AccountServer &s : self->m_servers) {
                        if (s.serverPublicId == id) {
                            const QJsonObject d = result.object();
                            s.name = d.value("name").toString(s.name);
                            s.state = d.value("state").toString(s.state);
                            s.ipAddress = d.value("ipAddress").toString();
                            const QString city = d.value("locationCity").toString();
                            s.location = city.isEmpty() ? d.value("location").toString() : city;
                            s.image = d.value("image").toString();
                            s.profile = d.value("profile").toString();
                            s.sshPort = d.value("sshPort").toInt(22);
                        }
                    }
                }
                finishOne();
            });
            m_api->get(serverPath(id, QStringLiteral("/access")), [self, generation, id, fingerprint, finishOne](const ApiResult &result) {
                if (self && generation == self->m_generation && result.isOk()) {
                    for (AccountServer &s : self->m_servers) {
                        if (s.serverPublicId == id) {
                            const QJsonObject a = result.object();
                            s.loginUsername = a.value("loginUsername").toString();
                            s.installState = a.value("installState").toString();
                            s.clientKeyFingerprint = a.value("clientKeyFingerprint").toString();
                            s.hostKeySha256 = sshHostKey::normalizeFingerprint(a.value("hostKeySha256").toString());
                            s.accessReady = accessIsReady(a, fingerprint);
                            const ServerFacts facts = self->m_purchases->facts(id);
                            s.passwordKnown = facts.sudoReady || !facts.loginPassword.isEmpty();
                        }
                    }
                }
                finishOne();
            });
        }
    }

    void Servers::requestDeviceAccess(const QString &serverPublicId, const Done &done)
    {
        QPointer<Servers> self(this);
        m_deviceKey->ensureRegistered(Session::defaultDeviceName(), [self, serverPublicId, done](const ApiResult &keyResult, const QString &keyId) {
            if (!self) {
                return;
            }
            if (keyId.isEmpty()) {
                done(keyResult);
                return;
            }
            self->m_api->put(serverPath(serverPublicId, QStringLiteral("/access/key")), QJsonObject { { "sshKeyPublicId", keyId } }, done);
        });
    }

    amnezia::ServerCredentials Servers::credentialsFor(const QString &serverPublicId) const
    {
        amnezia::ServerCredentials credentials;
        const AccountServer *server = find(serverPublicId);
        if (!server || !server->accessReady || server->ipAddress.isEmpty() || server->hostKeySha256.isEmpty()) {
            return credentials;
        }
        credentials.hostName = server->ipAddress;
        credentials.port = server->sshPort > 0 ? server->sshPort : 22;
        credentials.userName = server->loginUsername;
        credentials.secretData = m_deviceKey->keyPair().privateKey;
        credentials.hostKeySha256 = server->hostKeySha256;
        return credentials;
    }
} // namespace webpack
