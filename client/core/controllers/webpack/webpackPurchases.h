#ifndef WEBPACKPURCHASES_H
#define WEBPACKPURCHASES_H

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>

#include <functional>

#include "webpackApiResult.h"
#include "webpackOrderAttempt.h"

namespace webpack
{
    class ApiClient;
    class DeviceKey;
    class Store;

    // What CoCo knows about one server it bought (or set up) through webpack. Persisted encrypted.
    struct ServerFacts
    {
        QString serverPublicId;
        QString loginUsername;
        QString loginPassword; // only until the sudo setup succeeded
        QString hostKeySha256;
        bool sudoReady = false;

        QJsonObject toJson() const;
        static ServerFacts fromJson(const QJsonObject &json);
    };

    struct Purchase
    {
        enum class Stage { Ordering, Building, InstallingAccess, Ready, Failed };

        QString id; // local
        QString planPublicId;
        QString planName;
        QString regionPublicId;
        QString regionName;
        QString operatingSystemId; // empty: the plan's default
        double amount = 0;
        QString currencyCode;
        QString idempotencyKey;
        QString choice;
        QString loginUsername;
        QString loginPassword;
        QString sshKeyPublicId;
        QString orderPublicId;
        QString serverPublicId;
        Stage stage = Stage::Ordering;
        QString errorCode;
        QDateTime createdAtUtc;
        QDateTime stageSinceUtc;

        bool isActive() const { return stage == Stage::Building || stage == Stage::InstallingAccess; }
        QJsonObject toJson() const;
        static Purchase fromJson(const QJsonObject &json);
        static QString stageName(Stage stage);
    };

    // Pure helpers (tested).
    QString generateLoginPassword(int length = 24);   // letters and digits, at least one of each
    bool isValidLoginUsername(const QString &name);   // webpack's rule (Servers.md, D8)
    // What the access answer means for this device: ready only when installed, carrying this
    // device's key, and with a pinned host key.
    bool accessIsReady(const QJsonObject &access, const QString &deviceFingerprint);

    // Orders a server with this device's key and a CoCo-made login, then follows the build until the
    // server is ready to configure. Survives restarts (state in the Store); polls the order every
    // 4 s and the access every 10 s while something is in progress.
    class Purchases : public QObject
    {
        Q_OBJECT

    public:
        static constexpr const char *loginUsername = "coco";

        using Placed = std::function<void(const ApiResult &result, const Purchase &purchase)>;

        Purchases(ApiClient *api, DeviceKey *deviceKey, Store *store, QObject *parent = nullptr);

        QList<Purchase> purchases() const { return m_purchases; }
        bool hasPendingOrder() const { return m_attempt.isPending(); }

        // Places the order (paid from the wallet). The idempotency key is kept until a definite answer.
        void placeOrder(const QString &planPublicId, const QString &planName, const QString &regionPublicId,
                        const QString &regionName, const QString &operatingSystemId, double amount,
                        const QString &currencyCode, const Placed &done);
        void dismiss(const QString &purchaseId);

        // Facts of servers CoCo bought or prepared; keyed by serverPublicId.
        ServerFacts facts(const QString &serverPublicId) const;
        void saveFacts(const ServerFacts &facts);

        void resume();
        void setPollIntervals(int orderMs, int accessMs) { m_orderPollMs = orderMs; m_accessPollMs = accessMs; }

    signals:
        void changed();
        void serverReady(const QString &serverPublicId, const QString &planName);

    private:
        void poll();
        void pollOrder(int index);
        void pollAccess(int index);
        void setStage(Purchase &purchase, Purchase::Stage stage, const QString &errorCode = {});
        void load();
        void save();
        int indexOf(const QString &id) const;

        ApiClient *m_api;
        DeviceKey *m_deviceKey;
        Store *m_store;
        QList<Purchase> m_purchases;
        QHash<QString, ServerFacts> m_facts;
        OrderAttempt m_attempt;
        QTimer m_timer;
        int m_inFlight = 0;
        int m_orderPollMs = 4000;
        int m_accessPollMs = 10000;
    };
} // namespace webpack

#endif // WEBPACKPURCHASES_H
