#include "webpackIntegration.h"

#include <QDesktopServices>

#include "secureSettingsStore.h"
#include "webpackApiClient.h"
#include "webpackDeviceKey.h"
#include "webpackPurchases.h"
#include "webpackServers.h"
#include "webpackSession.h"
#include "webpackShop.h"

namespace webpack
{
    Integration::Integration(SecureQSettings *settings, QObject *parent)
        : QObject(parent), m_store(std::make_unique<SecureSettingsStore>(settings))
    {
        m_api = new ApiClient(nullptr, this);
#ifdef QT_DEBUG
        m_api->setAllowLoopbackHttp(true);
#endif
        const QString baseUrl = m_store->value(keys::baseUrl).toString();
        if (!baseUrl.isEmpty() && !m_api->setBaseUrl(QUrl(baseUrl))) {
            qWarning() << "webpack: ignoring a base URL that is not HTTPS";
        }

        m_session = new Session(m_api, m_store.get(), this);
        m_session->setUrlOpener([](const QUrl &url) { return QDesktopServices::openUrl(url); });
        m_api->setTokenSource(m_session);

        m_deviceKey = new DeviceKey(m_api, m_store.get(), this);
        m_shop = new Shop(m_api, this);
        m_purchases = new Purchases(m_api, m_deviceKey, m_store.get(), this);
        m_servers = new Servers(m_api, m_deviceKey, m_purchases, this);

        if (m_session->isSignedIn()) {
            m_purchases->resume();
        }
        connect(m_session, &Session::stateChanged, this, [this]() {
            if (m_session->isSignedIn()) {
                m_purchases->resume();
            }
        });
    }

    Integration::~Integration()
    {
        // Children that use the store go first.
        delete m_servers;
        delete m_purchases;
        delete m_shop;
        delete m_deviceKey;
        delete m_session;
        delete m_api;
    }
} // namespace webpack
