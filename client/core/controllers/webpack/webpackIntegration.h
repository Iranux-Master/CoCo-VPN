#ifndef WEBPACKINTEGRATION_H
#define WEBPACKINTEGRATION_H

#include <QObject>
#include <memory>

class SecureQSettings;

namespace webpack
{
    class ApiClient;
    class DeviceKey;
    class Purchases;
    class Servers;
    class Session;
    class Shop;
    class Store;

    // Composition root of the webpack.ir integration: one of each part, sharing one API client
    // and the encrypted store. Optional for the app: nothing here runs before the person signs in.
    class Integration : public QObject
    {
        Q_OBJECT

    public:
        explicit Integration(SecureQSettings *settings, QObject *parent = nullptr);
        ~Integration() override;

        ApiClient *api() const { return m_api; }
        Session *session() const { return m_session; }
        DeviceKey *deviceKey() const { return m_deviceKey; }
        Shop *shop() const { return m_shop; }
        Purchases *purchases() const { return m_purchases; }
        Servers *servers() const { return m_servers; }

    private:
        std::unique_ptr<Store> m_store;
        ApiClient *m_api;
        Session *m_session;
        DeviceKey *m_deviceKey;
        Shop *m_shop;
        Purchases *m_purchases;
        Servers *m_servers;
    };
} // namespace webpack

#endif // WEBPACKINTEGRATION_H
