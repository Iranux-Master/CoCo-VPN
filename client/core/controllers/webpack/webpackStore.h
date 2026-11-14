#ifndef WEBPACKSTORE_H
#define WEBPACKSTORE_H

#include <QHash>
#include <QString>
#include <QVariant>

namespace webpack
{
    // Where the webpack integration keeps its state. In the app this is SecureQSettings (see
    // SecureSettingsStore); the secret keys below are in SecureQSettings' encrypted list and are
    // not part of settings backups (backups only take "Conf/", "Servers/" and "Sharing/").
    class Store
    {
    public:
        virtual ~Store() = default;
        virtual QVariant value(const QString &key, const QVariant &defaultValue = {}) const = 0;
        virtual void setValue(const QString &key, const QVariant &value) = 0;
        virtual void remove(const QString &key) = 0;
    };

    namespace keys
    {
        inline const QString baseUrl = QStringLiteral("Webpack/baseUrl");
        // Secret: { sessionId, refreshToken, refreshTokenExpiresAtUtc, email, displayName, walletCurrency }.
        inline const QString session = QStringLiteral("Webpack/session");
        // Secret: this device's Ed25519 private key (OpenSSH PEM).
        inline const QString deviceKey = QStringLiteral("Webpack/deviceKey");
        // Secret: purchases in progress and per-server facts (login, password until sudo is set up, host key).
        inline const QString purchases = QStringLiteral("Webpack/purchases");
        inline const QString servers = QStringLiteral("Webpack/servers");
    } // namespace keys

    // For tests.
    class MemoryStore : public Store
    {
    public:
        QVariant value(const QString &key, const QVariant &defaultValue = {}) const override
        {
            return m_values.value(key, defaultValue);
        }
        void setValue(const QString &key, const QVariant &value) override { m_values.insert(key, value); }
        void remove(const QString &key) override { m_values.remove(key); }
        bool contains(const QString &key) const { return m_values.contains(key); }

    private:
        QHash<QString, QVariant> m_values;
    };
} // namespace webpack

#endif // WEBPACKSTORE_H
