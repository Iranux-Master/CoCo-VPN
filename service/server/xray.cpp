#include "xray.h"
#include "core/utils/networkUtilities.h"
#ifdef Q_OS_MAC
#include "router_mac.h"
#endif

#include <QDebug>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QCoreApplication>
#include <amnezia_xray.h>
#include <qdebug.h>

#ifdef Q_OS_DARWIN
    #include <arpa/inet.h>
    #include <cerrno>
    #include <cstddef>
    #include <cstdint>
    #include <cstring>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <sys/socket.h>
#endif
#ifdef Q_OS_WIN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif
#ifdef Q_OS_LINUX
    #include <sys/socket.h>
    #include "xray_defs.h"
#endif

namespace
{
    // CoCo DNS tunnel: its XRay config only talks to dnstt-client on 127.0.0.1, which already keeps its own DNS
    // queries outside the tunnel. Binding those sockets to the physical interface (below) would break a loopback
    // connection, so the binding is skipped when every outbound server is a loopback address.
    bool outboundsAreLoopbackOnly(const QString &cfg)
    {
        const QJsonArray outbounds = QJsonDocument::fromJson(cfg.toUtf8()).object().value(QStringLiteral("outbounds")).toArray();
        if (outbounds.isEmpty()) {
            return false;
        }
        for (const QJsonValue &v : outbounds) {
            const QJsonArray vnext = v.toObject().value(QStringLiteral("settings")).toObject().value(QStringLiteral("vnext")).toArray();
            if (vnext.isEmpty()) {
                return false;
            }
            for (const QJsonValue &server : vnext) {
                if (!QHostAddress(server.toObject().value(QStringLiteral("address")).toString()).isLoopback()) {
                    return false;
                }
            }
        }
        return true;
    }
}

bool Xray::startXray(const QString &cfg)
{
    qDebug() << "Xray::startXray()";
    const auto gatewayAndIface = NetworkUtilities::getGatewayAndIface();
    const QString defaultGateway = gatewayAndIface.first;
    const QNetworkInterface defaultIface = gatewayAndIface.second;
#ifdef Q_OS_LINUX
    m_defaultIfaceName = defaultIface.name().toUtf8();
#else
    m_defaultIfaceIdx = defaultIface.index();
#endif
    if (outboundsAreLoopbackOnly(cfg)) {
        qDebug() << "[xray] loopback-only outbounds (DNS tunnel): sockets are not bound to the uplink interface";
#ifdef Q_OS_LINUX
        m_defaultIfaceName.clear();
#else
        m_defaultIfaceIdx = 0;
#endif
    } else if (defaultIface.index() > 0) {
        qDebug() << "[xray] using uplink interface:" << defaultIface.name() << "(" << defaultIface.index() << ")";
    }

#ifdef Q_OS_MAC
    m_uplinkIfaceName = defaultIface.name();
    m_uplinkGateway = defaultGateway;
    if (!m_uplinkIfaceName.isEmpty()) {
        const bool installed = RouterMac::Instance().routeAddXray(m_uplinkIfaceName, m_uplinkGateway);
        if (!installed) {
            qWarning() << "[xray] failed to install xray routes on" << m_uplinkIfaceName;
        }
    }
#endif

    if (auto err = amnezia_xray_setsockcallback(ctxSockCallback, this); err != nullptr) {
        qDebug() << "[xray] sockopt failed: " << err;
        amnezia_xray_free(err);
        return false;
    }

    amnezia_xray_setloghandler(ctxLogHandler, this);

    QByteArray bytes = cfg.toUtf8();
    if (auto err = amnezia_xray_configure(bytes.data()); err != nullptr) {
        qDebug() << "[xray] configuration failed: " << err;
        amnezia_xray_free(err);
        return false;
    }

    if (auto err = amnezia_xray_start(); err != nullptr) {
        qDebug() << "[xray] failed to start: " << err;
        amnezia_xray_free(err);
        return false;
    }

    return true;
}

bool Xray::stopXray()
{
    qDebug() << "Xray::stopXray()";
    bool success = true;
    if (auto err = amnezia_xray_stop(); err != nullptr) {
        qDebug() << "[xray] failed to stop: " << err;
        amnezia_xray_free(err);
        success = false;
    }

#ifdef Q_OS_MAC
    if (!m_uplinkIfaceName.isEmpty()) {
        RouterMac::Instance().routeDeleteXray(m_uplinkIfaceName, m_uplinkGateway);
    }
    m_uplinkIfaceName.clear();
    m_uplinkGateway.clear();
#endif

    return success;
}

void Xray::logHandler(char* str)
{
    QMetaObject::invokeMethod(qApp, [str = QString::fromUtf8(str)] {
        qDebug() << "[xray]" << str;
    }, Qt::QueuedConnection);
}

void Xray::sockCallback(uintptr_t fd)
{
#ifdef Q_OS_MAC
    if (m_defaultIfaceIdx > 0) {
        setsockopt(fd, IPPROTO_IP, IP_BOUND_IF, &m_defaultIfaceIdx, sizeof(m_defaultIfaceIdx));
        setsockopt(fd, IPPROTO_IPV6, IPV6_BOUND_IF, &m_defaultIfaceIdx, sizeof(m_defaultIfaceIdx));
    }
#endif
#ifdef Q_OS_WIN
    if (DWORD idx = m_defaultIfaceIdx; idx > 0) {
        setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_IF, reinterpret_cast<char *>(&idx), sizeof(idx));
        idx = htonl(idx); // IP_UNICAST_IF expects index in network byte order
        setsockopt(fd, IPPROTO_IP, IP_UNICAST_IF, reinterpret_cast<char *>(&idx), sizeof(idx));
    }
#endif
#ifdef Q_OS_LINUX
    if (!m_defaultIfaceName.isEmpty()) {
        setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, m_defaultIfaceName.data(), m_defaultIfaceName.size());
        setsockopt(fd, SOL_SOCKET, SO_MARK, &amnezia::xray::xrayTrafficMark, sizeof(amnezia::xray::xrayTrafficMark));
    }
#endif
}
