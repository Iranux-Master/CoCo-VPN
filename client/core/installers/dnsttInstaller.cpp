#include "dnsttInstaller.h"

#include "core/models/protocols/xrayProtocolConfig.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/dnstt/dnsttUtils.h"
#include "core/utils/selfhosted/sshSession.h"
#include "logger.h"

namespace
{
    Logger logger("DnsttInstaller");

    QString readTrimmed(SshSession *sshSession, amnezia::DockerContainer container, const amnezia::ServerCredentials &credentials,
                        const char *path)
    {
        amnezia::ErrorCode errorCode = amnezia::ErrorCode::NoError;
        const QString value =
                QString::fromUtf8(sshSession->getTextFileFromContainer(container, credentials, QString::fromLatin1(path), errorCode));
        return errorCode == amnezia::ErrorCode::NoError ? value.trimmed() : QString();
    }
}

using namespace amnezia;

DnsttInstaller::DnsttInstaller(QObject *parent) : XrayInstaller(parent)
{
}

ErrorCode DnsttInstaller::extractConfigFromContainer(DockerContainer container, const ServerCredentials &credentials,
                                                     SshSession *sshSession, ContainerConfig &config)
{
    const ErrorCode errorCode = XrayInstaller::extractConfigFromContainer(container, credentials, sshSession, config);
    if (errorCode != ErrorCode::NoError) {
        return errorCode;
    }

    auto *xrayConfig = config.getXrayProtocolConfig();
    if (!xrayConfig) {
        return ErrorCode::InternalError;
    }

    XrayServerConfig &srv = xrayConfig->serverConfig;
    // The inbound port read above is the loopback forward port; the port people reach is always UDP 53.
    srv.port = QString(protocols::dnstt::defaultPort);
    srv.transportProto = QStringLiteral("udp");
    srv.security = QStringLiteral("none");
    srv.flow.clear();

    const QString domain = dnstt::normalizeTunnelDomain(readTrimmed(sshSession, container, credentials, protocols::dnstt::domainPath));
    if (dnstt::isValidTunnelDomain(domain)) {
        srv.dnstt.domain = domain;
    } else {
        logger.warning() << "DNS tunnel domain file is missing or invalid";
    }
    srv.dnstt.mtu = dnstt::normalizeMtu(readTrimmed(sshSession, container, credentials, protocols::dnstt::mtuPath));

    const QString publicKey = readTrimmed(sshSession, container, credentials, protocols::dnstt::publicKeyPath).toLower();
    if (dnstt::isValidPublicKey(publicKey)) {
        srv.dnstt.publicKey = publicKey;
    }
    return ErrorCode::NoError;
}
