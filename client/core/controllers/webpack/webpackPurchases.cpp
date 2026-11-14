#include "webpackPurchases.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>

#include "core/utils/selfhosted/sshHostKey.h"
#include "webpackApiClient.h"
#include "webpackDeviceKey.h"
#include "webpackSession.h"
#include "webpackStore.h"

namespace webpack
{
    namespace
    {
        constexpr int orderTimeoutMs = 120000;

        QString escapeId(const QString &id)
        {
            return QString::fromLatin1(QUrl::toPercentEncoding(id));
        }

        Purchase::Stage stageFrom(const QString &name)
        {
            if (name == QLatin1String("building")) return Purchase::Stage::Building;
            if (name == QLatin1String("installingAccess")) return Purchase::Stage::InstallingAccess;
            if (name == QLatin1String("ready")) return Purchase::Stage::Ready;
            if (name == QLatin1String("failed")) return Purchase::Stage::Failed;
            return Purchase::Stage::Ordering;
        }

        QString existingOrderId(const QString &message)
        {
            static const QRegularExpression pattern(QStringLiteral("ExistingOrder:\\s*([A-Za-z0-9_-]+)"));
            const auto match = pattern.match(message);
            return match.hasMatch() ? match.captured(1) : QString();
        }
    } // namespace

    QJsonObject ServerFacts::toJson() const
    {
        QJsonObject json { { "serverPublicId", serverPublicId }, { "loginUsername", loginUsername }, { "hostKeySha256", hostKeySha256 },
                           { "sudoReady", sudoReady } };
        if (!loginPassword.isEmpty()) {
            json.insert("loginPassword", loginPassword);
        }
        return json;
    }

    ServerFacts ServerFacts::fromJson(const QJsonObject &json)
    {
        ServerFacts facts;
        facts.serverPublicId = json.value("serverPublicId").toString();
        facts.loginUsername = json.value("loginUsername").toString();
        facts.loginPassword = json.value("loginPassword").toString();
        facts.hostKeySha256 = json.value("hostKeySha256").toString();
        facts.sudoReady = json.value("sudoReady").toBool();
        return facts;
    }

    QString Purchase::stageName(Stage stage)
    {
        switch (stage) {
        case Stage::Ordering: return QStringLiteral("ordering");
        case Stage::Building: return QStringLiteral("building");
        case Stage::InstallingAccess: return QStringLiteral("installingAccess");
        case Stage::Ready: return QStringLiteral("ready");
        case Stage::Failed: return QStringLiteral("failed");
        }
        return QStringLiteral("ordering");
    }

    QJsonObject Purchase::toJson() const
    {
        return QJsonObject { { "id", id },
                             { "planPublicId", planPublicId },
                             { "planName", planName },
                             { "regionPublicId", regionPublicId },
                             { "regionName", regionName },
                             { "operatingSystemId", operatingSystemId },
                             { "amount", amount },
                             { "currencyCode", currencyCode },
                             { "idempotencyKey", idempotencyKey },
                             { "choice", choice },
                             { "loginUsername", loginUsername },
                             { "loginPassword", loginPassword },
                             { "sshKeyPublicId", sshKeyPublicId },
                             { "orderPublicId", orderPublicId },
                             { "serverPublicId", serverPublicId },
                             { "stage", stageName(stage) },
                             { "errorCode", errorCode },
                             { "createdAtUtc", createdAtUtc.toString(Qt::ISODateWithMs) },
                             { "stageSinceUtc", stageSinceUtc.toString(Qt::ISODateWithMs) } };
    }

    Purchase Purchase::fromJson(const QJsonObject &json)
    {
        Purchase p;
        p.id = json.value("id").toString();
        p.planPublicId = json.value("planPublicId").toString();
        p.planName = json.value("planName").toString();
        p.regionPublicId = json.value("regionPublicId").toString();
        p.regionName = json.value("regionName").toString();
        p.operatingSystemId = json.value("operatingSystemId").toString();
        p.amount = json.value("amount").toDouble();
        p.currencyCode = json.value("currencyCode").toString();
        p.idempotencyKey = json.value("idempotencyKey").toString();
        p.choice = json.value("choice").toString();
        p.loginUsername = json.value("loginUsername").toString();
        p.loginPassword = json.value("loginPassword").toString();
        p.sshKeyPublicId = json.value("sshKeyPublicId").toString();
        p.orderPublicId = json.value("orderPublicId").toString();
        p.serverPublicId = json.value("serverPublicId").toString();
        p.stage = stageFrom(json.value("stage").toString());
        p.errorCode = json.value("errorCode").toString();
        p.createdAtUtc = QDateTime::fromString(json.value("createdAtUtc").toString(), Qt::ISODateWithMs);
        p.stageSinceUtc = QDateTime::fromString(json.value("stageSinceUtc").toString(), Qt::ISODateWithMs);
        return p;
    }

    QString generateLoginPassword(int length)
    {
        static const QString letters = QStringLiteral("abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ");
        static const QString digits = QStringLiteral("23456789");
        const QString all = letters + digits;
        length = qBound(12, length, 64);
        QRandomGenerator *random = QRandomGenerator::system();
        QString password;
        password.reserve(length);
        for (int i = 0; i < length; ++i) {
            password.append(all.at(random->bounded(all.size())));
        }
        // webpack's rule: at least one letter and one digit.
        password[random->bounded(length)] = letters.at(random->bounded(letters.size()));
        int digitAt = random->bounded(length);
        password[digitAt] = digits.at(random->bounded(digits.size()));
        bool hasLetter = false;
        for (const QChar c : password) {
            hasLetter = hasLetter || c.isLetter();
        }
        if (!hasLetter) {
            password[(digitAt + 1) % length] = letters.at(0);
        }
        return password;
    }

    bool isValidLoginUsername(const QString &name)
    {
        static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9_-]{2,31}$"));
        static const QStringList reserved { "root", "admin", "www-data", "ubuntu", "debian", "daemon", "bin", "sys", "nobody", "sshd" };
        return pattern.match(name).hasMatch() && !reserved.contains(name);
    }

    bool accessIsReady(const QJsonObject &access, const QString &deviceFingerprint)
    {
        if (access.value("installState").toString() != QLatin1String("installed")) {
            return false;
        }
        if (sshHostKey::normalizeFingerprint(access.value("hostKeySha256").toString()).isEmpty()) {
            return false;
        }
        const QString clientKey = sshHostKey::normalizeFingerprint(access.value("clientKeyFingerprint").toString());
        return !clientKey.isEmpty() && clientKey == sshHostKey::normalizeFingerprint(deviceFingerprint);
    }

    Purchases::Purchases(ApiClient *api, DeviceKey *deviceKey, Store *store, QObject *parent)
        : QObject(parent), m_api(api), m_deviceKey(deviceKey), m_store(store)
    {
        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, &Purchases::poll);
        load();
    }

    int Purchases::indexOf(const QString &id) const
    {
        for (int i = 0; i < m_purchases.size(); ++i) {
            if (m_purchases.at(i).id == id) {
                return i;
            }
        }
        return -1;
    }

    void Purchases::placeOrder(const QString &planPublicId, const QString &planName, const QString &regionPublicId,
                               const QString &regionName, const QString &operatingSystemId, double amount,
                               const QString &currencyCode, const Placed &done)
    {
        const QString key = m_attempt.keyFor(planPublicId, regionPublicId, operatingSystemId, QString::number(amount, 'f', 2), currencyCode);

        // A kept key belongs to an earlier, unclear order: send that order again, unchanged.
        int index = -1;
        for (int i = 0; i < m_purchases.size(); ++i) {
            if (m_purchases.at(i).idempotencyKey == key) {
                index = i;
            }
        }
        if (index < 0) {
            Purchase p;
            p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            p.planPublicId = planPublicId;
            p.planName = planName;
            p.regionPublicId = regionPublicId;
            p.regionName = regionName;
            p.operatingSystemId = operatingSystemId;
            p.amount = amount;
            p.currencyCode = currencyCode;
            p.idempotencyKey = key;
            p.choice = m_attempt.currentChoice();
            p.loginUsername = QString::fromLatin1(loginUsername);
            p.loginPassword = generateLoginPassword();
            p.createdAtUtc = QDateTime::currentDateTimeUtc();
            p.stageSinceUtc = p.createdAtUtc;
            m_purchases.append(p);
            index = m_purchases.size() - 1;
            save();
        }
        const QString purchaseId = m_purchases.at(index).id;

        QPointer<Purchases> self(this);
        m_deviceKey->ensureRegistered(Session::defaultDeviceName(), [self, purchaseId, done](const ApiResult &keyResult, const QString &keyId) {
            if (!self) {
                return;
            }
            const int i = self->indexOf(purchaseId);
            if (i < 0) {
                return;
            }
            if (keyId.isEmpty()) {
                // Nothing was sent to the order route: the key stays for the next press.
                done(keyResult, self->m_purchases.at(i));
                return;
            }
            Purchase &p = self->m_purchases[i];
            p.sshKeyPublicId = keyId;
            QJsonObject body { { "planPublicId", p.planPublicId },
                               { "regionPublicId", p.regionPublicId },
                               { "idempotencyKey", p.idempotencyKey },
                               { "sshKeyPublicId", p.sshKeyPublicId },
                               { "loginUsername", p.loginUsername },
                               { "loginPassword", p.loginPassword },
                               { "expectedAmount", p.amount },
                               { "expectedCurrencyCode", p.currencyCode } };
            if (!p.operatingSystemId.isEmpty()) {
                body.insert("operatingSystemId", p.operatingSystemId);
            }

            self->m_api->post(QStringLiteral("api/v1/vps/orders"), body, [self, purchaseId, done](const ApiResult &result) {
                if (!self) {
                    return;
                }
                self->m_attempt.after(result);
                const int j = self->indexOf(purchaseId);
                if (j < 0) {
                    return;
                }
                Purchase &placed = self->m_purchases[j];

                if (result.isOk()) {
                    const QJsonObject data = result.object();
                    placed.orderPublicId = data.value("orderPublicId").toString();
                    placed.serverPublicId = data.value("serverPublicId").toString();
                    ServerFacts facts;
                    facts.serverPublicId = placed.serverPublicId;
                    facts.loginUsername = placed.loginUsername;
                    facts.loginPassword = placed.loginPassword;
                    self->saveFacts(facts);
                    self->setStage(placed, data.value("status").toString() == QLatin1String("succeeded")
                                           ? Purchase::Stage::InstallingAccess
                                           : Purchase::Stage::Building);
                } else if (result.errorCode == QLatin1String("order_pending_review")) {
                    // Charged and being checked by webpack: follow that order; never charge again.
                    placed.orderPublicId = existingOrderId(result.message);
                    self->setStage(placed, placed.orderPublicId.isEmpty() ? Purchase::Stage::Ordering : Purchase::Stage::Building,
                                   result.errorCode);
                } else if (OrderAttempt::nothingCharged(result)) {
                    const Purchase copy = placed;
                    self->m_purchases.removeAt(j);
                    self->save();
                    emit self->changed();
                    done(result, copy);
                    return;
                } else {
                    // Unclear: the key is kept, the next press sends the same order.
                    placed.errorCode = OrderAttempt::refusalKey(result);
                    self->save();
                    emit self->changed();
                }
                const Purchase copy = self->m_purchases.at(j);
                done(result, copy);
                self->resume();
            }, { true, orderTimeoutMs });
        });
    }

    void Purchases::dismiss(const QString &purchaseId)
    {
        const int i = indexOf(purchaseId);
        if (i < 0 || m_purchases.at(i).isActive() || m_purchases.at(i).stage == Purchase::Stage::Ordering) {
            return; // never forget an order that may be charged
        }
        m_purchases.removeAt(i);
        save();
        emit changed();
    }

    ServerFacts Purchases::facts(const QString &serverPublicId) const
    {
        return m_facts.value(serverPublicId);
    }

    void Purchases::saveFacts(const ServerFacts &facts)
    {
        if (facts.serverPublicId.isEmpty()) {
            return;
        }
        m_facts.insert(facts.serverPublicId, facts);
        save();
    }

    void Purchases::resume()
    {
        for (const Purchase &p : m_purchases) {
            if (p.isActive()) {
                if (!m_timer.isActive() && m_inFlight == 0) {
                    m_timer.start(p.stage == Purchase::Stage::Building ? m_orderPollMs : m_accessPollMs);
                }
                return;
            }
        }
    }

    void Purchases::poll()
    {
        for (int i = 0; i < m_purchases.size(); ++i) {
            const Purchase &p = m_purchases.at(i);
            if (p.stage == Purchase::Stage::Building) {
                pollOrder(i);
            } else if (p.stage == Purchase::Stage::InstallingAccess) {
                pollAccess(i);
            }
        }
        if (m_inFlight == 0) {
            resume();
        }
    }

    void Purchases::pollOrder(int index)
    {
        const QString id = m_purchases.at(index).id;
        ++m_inFlight;
        QPointer<Purchases> self(this);
        m_api->get(QStringLiteral("api/v1/vps/orders/") + escapeId(m_purchases.at(index).orderPublicId), [self, id](const ApiResult &result) {
            if (!self) {
                return;
            }
            --self->m_inFlight;
            const int i = self->indexOf(id);
            if (i >= 0 && result.isOk()) {
                Purchase &p = self->m_purchases[i];
                const QJsonObject data = result.object();
                const QString status = data.value("status").toString();
                if (p.serverPublicId.isEmpty()) {
                    p.serverPublicId = data.value("serverPublicId").toString();
                    if (!p.serverPublicId.isEmpty() && !self->m_facts.contains(p.serverPublicId)) {
                        self->saveFacts({ p.serverPublicId, p.loginUsername, p.loginPassword, {}, false });
                    }
                }
                if (status == QLatin1String("succeeded")) {
                    self->setStage(p, Purchase::Stage::InstallingAccess);
                } else if (status == QLatin1String("failed")) {
                    self->setStage(p, Purchase::Stage::Failed, QStringLiteral("order_failed"));
                }
            }
            if (self->m_inFlight == 0) {
                self->resume();
            }
        });
    }

    void Purchases::pollAccess(int index)
    {
        const QString id = m_purchases.at(index).id;
        ++m_inFlight;
        QPointer<Purchases> self(this);
        const QString path = QStringLiteral("api/v1/vps/servers/%1/access").arg(escapeId(m_purchases.at(index).serverPublicId));
        m_api->get(path, [self, id](const ApiResult &result) {
            if (!self) {
                return;
            }
            --self->m_inFlight;
            const int i = self->indexOf(id);
            if (i >= 0 && result.isOk()) {
                Purchase &p = self->m_purchases[i];
                const QJsonObject access = result.object();
                if (accessIsReady(access, self->m_deviceKey->keyPair().fingerprint)) {
                    ServerFacts facts = self->facts(p.serverPublicId);
                    facts.serverPublicId = p.serverPublicId;
                    facts.loginUsername = access.value("loginUsername").toString(p.loginUsername);
                    if (facts.loginPassword.isEmpty()) {
                        facts.loginPassword = p.loginPassword;
                    }
                    facts.hostKeySha256 = sshHostKey::normalizeFingerprint(access.value("hostKeySha256").toString());
                    self->saveFacts(facts);
                    // The password now lives in the server facts only.
                    p.loginPassword.clear();
                    self->setStage(p, Purchase::Stage::Ready);
                    emit self->serverReady(p.serverPublicId, p.planName);
                } else if (access.value("installState").toString() == QLatin1String("failed")) {
                    self->setStage(p, Purchase::Stage::Failed, QStringLiteral("access_failed"));
                }
            }
            if (self->m_inFlight == 0) {
                self->resume();
            }
        });
    }

    void Purchases::setStage(Purchase &purchase, Purchase::Stage stage, const QString &errorCode)
    {
        purchase.stage = stage;
        purchase.errorCode = errorCode;
        purchase.stageSinceUtc = QDateTime::currentDateTimeUtc();
        save();
        emit changed();
    }

    void Purchases::load()
    {
        const QJsonObject root = QJsonDocument::fromJson(m_store->value(keys::purchases).toByteArray()).object();
        for (const QJsonValue &value : root.value("purchases").toArray()) {
            const Purchase p = Purchase::fromJson(value.toObject());
            if (!p.id.isEmpty()) {
                m_purchases.append(p);
                if (p.stage == Purchase::Stage::Ordering && !p.idempotencyKey.isEmpty()) {
                    m_attempt.restore(p.idempotencyKey, p.choice, true);
                }
            }
        }
        const QJsonObject servers = QJsonDocument::fromJson(m_store->value(keys::servers).toByteArray()).object();
        for (auto it = servers.begin(); it != servers.end(); ++it) {
            m_facts.insert(it.key(), ServerFacts::fromJson(it.value().toObject()));
        }
    }

    void Purchases::save()
    {
        QJsonArray list;
        for (const Purchase &p : m_purchases) {
            list.append(p.toJson());
        }
        m_store->setValue(keys::purchases, QJsonDocument(QJsonObject { { "purchases", list } }).toJson(QJsonDocument::Compact));
        QJsonObject servers;
        for (auto it = m_facts.begin(); it != m_facts.end(); ++it) {
            servers.insert(it.key(), it.value().toJson());
        }
        m_store->setValue(keys::servers, QJsonDocument(servers).toJson(QJsonDocument::Compact));
    }
} // namespace webpack
