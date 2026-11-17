#include "dnsttProtocol.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include "core/protocols/protocolUtils.h"
#include "core/protocols/xrayProtocol.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/ipcClient.h"
#include "core/utils/networkUtilities.h"
#include "core/utils/utilities.h"
#include "logger.h"

#ifdef Q_OS_WIN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
#endif

namespace
{
    Logger logger("DnsttProtocol");

    constexpr int clientListenTimeoutMs = 10000;
}

using namespace amnezia;

DnsttProtocol::DnsttProtocol(const QJsonObject &configuration, QObject *parent) : VpnProtocol(configuration, parent)
{
    m_routeGateway = NetworkUtilities::getGatewayAndIface().first;
    m_vpnGateway = protocols::xray::defaultLocalAddr;
    m_vpnLocalAddress = protocols::xray::defaultLocalAddr;

    const QJsonObject configData = configuration.value(ProtocolUtils::key_proto_config_data(Proto::Dnstt)).toObject();
    m_clientConfig = QJsonDocument::fromJson(configData.value(configKey::config).toString().toUtf8()).object();

    const QJsonObject block = m_clientConfig.value(QLatin1String(protocols::dnstt::block)).toObject();
    m_domain = dnstt::normalizeTunnelDomain(block.value(QLatin1String(protocols::dnstt::domain)).toString());
    m_publicKey = block.value(QLatin1String(protocols::dnstt::publicKey)).toString().trimmed().toLower();

    // Resolvers named in the config come first (operator's choice), then the network's own and the public ones.
    for (const QJsonValue &v : block.value(QLatin1String(protocols::dnstt::resolvers)).toArray()) {
        const dnstt::Resolver r = dnstt::parseResolver(v.toString());
        if (r.isValid()) {
            m_resolvers.append(r);
        }
    }
    m_resolvers.append(dnstt::defaultResolvers(systemDnsServers()));
}

DnsttProtocol::~DnsttProtocol()
{
    DnsttProtocol::stop();
}

ErrorCode DnsttProtocol::start()
{
    m_stopping = false;
    if (!dnstt::isValidTunnelDomain(m_domain) || !dnstt::isValidPublicKey(m_publicKey)) {
        logger.error() << "DNS tunnel config has no valid domain or public key";
        return ErrorCode::ImportInvalidConfigError;
    }
    if (!QFileInfo::exists(Utils::executable(QStringLiteral("dnstt-client"), true))) {
        return ErrorCode::DnsttClientExecutableMissing;
    }
    if (m_resolvers.isEmpty()) {
        return ErrorCode::DnsttResolverUnreachable;
    }

    setConnectionState(Vpn::ConnectionState::Connecting);

    // The first resolver is used. dnstt cannot tell whether a resolver passes the tunnel until traffic flows, so a
    // resolver that fails is changed in the config (the "resolvers" list) rather than guessed here.
    const dnstt::Resolver resolver = m_resolvers.first();
    logger.info() << "DNS tunnel via resolver" << dnstt::resolverToSpec(resolver);

    if (!addResolverRoute(resolver.ip)) {
        logger.warning() << "Could not add the route for the DNS resolver; continuing";
    }

    const int localPort = freeLocalPort();
    if (localPort <= 0) {
        removeResolverRoute();
        return ErrorCode::DnsttClientFailed;
    }

    if (ErrorCode e = startDnsttClient(resolver, localPort); e != ErrorCode::NoError) {
        removeResolverRoute();
        return e;
    }

    const QJsonObject xrayConfig = dnstt::xrayConfigForLocalPort(m_clientConfig, localPort);
    if (xrayConfig.isEmpty()) {
        stopDnsttClient();
        removeResolverRoute();
        return ErrorCode::ImportInvalidConfigError;
    }

    // The inner XRay protocol sees the resolver as its server: XrayProtocol exempts that address from the kill switch
    // and the Windows split tunnel.
    QJsonObject innerConfiguration = m_rawConfig;
    innerConfiguration.remove(ProtocolUtils::key_proto_config_data(Proto::Dnstt));
    innerConfiguration.insert(ProtocolUtils::key_proto_config_data(Proto::Xray),
                              QJsonObject { { configKey::config,
                                              QString::fromUtf8(QJsonDocument(xrayConfig).toJson(QJsonDocument::Compact)) } });
    innerConfiguration[configKey::hostName] = resolver.ip;

    m_xray.reset(new XrayProtocol(innerConfiguration));
    connect(m_xray.data(), &VpnProtocol::connectionStateChanged, this, &DnsttProtocol::onInnerStateChanged);
    connect(m_xray.data(), &VpnProtocol::bytesChanged, this, &VpnProtocol::setBytesChanged);
    connect(m_xray.data(), &VpnProtocol::protocolError, this, &VpnProtocol::protocolError);

    const ErrorCode e = m_xray->start();
    if (e != ErrorCode::NoError) {
        stop();
        return e;
    }
    return ErrorCode::NoError;
}

void DnsttProtocol::stop()
{
    m_stopping = true;
    if (m_xray) {
        m_xray->disconnect(this);
        m_xray->stop();
        m_xray.reset();
    }
    stopDnsttClient();
    removeResolverRoute();
    if (m_connectionState != Vpn::ConnectionState::Disconnected && m_connectionState != Vpn::ConnectionState::Error) {
        setConnectionState(Vpn::ConnectionState::Disconnected);
    }
}

void DnsttProtocol::onInnerStateChanged(Vpn::ConnectionState state)
{
    if (state == Vpn::ConnectionState::Error && m_xray) {
        setLastError(m_xray->lastError());
        return;
    }
    setConnectionState(state);
}

ErrorCode DnsttProtocol::startDnsttClient(const dnstt::Resolver &resolver, int localPort)
{
    m_dnsttProcess.reset(new QProcess());
    m_dnsttProcess->setProgram(Utils::executable(QStringLiteral("dnstt-client"), true));
    m_dnsttProcess->setArguments(dnstt::clientArguments(resolver, m_publicKey, m_domain, localPort));
    m_dnsttProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_dnsttProcess.data(), &QProcess::readyRead, this, [this]() {
        // dnstt-client logs sessions and errors; it never prints keys or traffic.
        const QString out = QString::fromUtf8(m_dnsttProcess->readAll()).trimmed();
        if (!out.isEmpty()) {
            logger.info() << "[dnstt-client]" << out;
        }
    });
    connect(m_dnsttProcess.data(), &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        if (m_stopping) {
            return;
        }
        logger.error() << "dnstt-client exited, code" << exitCode;
        stop();
        setLastError(ErrorCode::DnsttClientFailed);
    });

    m_dnsttProcess->start();
    if (!m_dnsttProcess->waitForStarted(3000)) {
        logger.error() << "dnstt-client did not start:" << m_dnsttProcess->errorString();
        m_dnsttProcess.reset();
        return ErrorCode::DnsttClientFailed;
    }

    // Wait until it accepts on the local port (it listens at once; the tunnel itself opens with the first stream).
    for (int waited = 0; waited < clientListenTimeoutMs; waited += 200) {
        QTcpSocket probe;
        probe.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(localPort));
        if (probe.waitForConnected(200)) {
            probe.abort();
            return ErrorCode::NoError;
        }
        if (m_dnsttProcess->state() == QProcess::NotRunning) {
            break;
        }
        QThread::msleep(200);
    }
    logger.error() << "dnstt-client is not listening on the local port";
    stopDnsttClient();
    return ErrorCode::DnsttClientFailed;
}

void DnsttProtocol::stopDnsttClient()
{
    if (!m_dnsttProcess) {
        return;
    }
    m_dnsttProcess->disconnect(this);
    if (m_dnsttProcess->state() != QProcess::NotRunning) {
        m_dnsttProcess->kill();
        m_dnsttProcess->waitForFinished(2000);
    }
    m_dnsttProcess.reset();
}

bool DnsttProtocol::addResolverRoute(const QString &ip)
{
    if (m_routeGateway.isEmpty() || ip.isEmpty() || QHostAddress(ip).isLoopback()) {
        return false;
    }
    return IpcClient::withInterface(
            [&](QSharedPointer<IpcInterfaceReplica> iface) {
                auto reply = iface->routeAddList(m_routeGateway, QStringList { ip });
                if (!reply.waitForFinished() || reply.returnValue() != 1) {
                    return false;
                }
                m_routedResolverIp = ip;
                return true;
            },
            []() { return false; });
}

void DnsttProtocol::removeResolverRoute()
{
    if (m_routedResolverIp.isEmpty()) {
        return;
    }
    const QString ip = m_routedResolverIp;
    m_routedResolverIp.clear();
    IpcClient::withInterface([&](QSharedPointer<IpcInterfaceReplica> iface) {
        auto reply = iface->routeDeleteList(m_routeGateway, QStringList { ip });
        reply.waitForFinished();
    });
}

int DnsttProtocol::freeLocalPort()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return -1;
    }
    const int port = server.serverPort();
    server.close();
    return port;
}

QStringList DnsttProtocol::systemDnsServers()
{
    QStringList servers;
#ifdef Q_OS_WIN
    // The DNS servers of the adapter that holds the default route (the ISP's or the router's resolver).
    const int ifaceIndex = NetworkUtilities::getGatewayAndIface().second.index();
    ULONG size = 15000;
    QByteArray buffer(static_cast<int>(size), 0);
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME;
    ULONG result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(static_cast<int>(size));
        result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    }
    if (result != NO_ERROR) {
        return servers;
    }
    for (auto *a = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()); a; a = a->Next) {
        if (ifaceIndex > 0 && static_cast<int>(a->IfIndex) != ifaceIndex) {
            continue;
        }
        for (auto *dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
            const QHostAddress address(dns->Address.lpSockaddr);
            // IPv6 site-local fec0:: placeholders and loopback are not real resolvers.
            if (!address.isNull() && !address.isLoopback() && !address.toString().startsWith(QLatin1String("fec0:"))) {
                servers.append(address.toString());
            }
        }
    }
#else
    QFile resolvConf(QStringLiteral("/etc/resolv.conf"));
    if (resolvConf.open(QIODevice::ReadOnly)) {
        for (const QByteArray &line : resolvConf.readAll().split('\n')) {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2 && parts.at(0) == "nameserver") {
                const QHostAddress address(QString::fromLatin1(parts.at(1)));
                if (!address.isNull() && !address.isLoopback()) {
                    servers.append(address.toString());
                }
            }
        }
    }
#endif
    return servers;
}
