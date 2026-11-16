#ifndef DNSTTINSTALLER_H
#define DNSTTINSTALLER_H

#include "xrayInstaller.h"

// CoCo DNS tunnel container (docs/plans/D1-dns-tunnel.md). The XRay part inside it is read like the XRay container;
// the tunnel domain, MTU and dnstt public key come from the files configure_container.sh writes.
class DnsttInstaller : public XrayInstaller
{
    Q_OBJECT
public:
    explicit DnsttInstaller(QObject *parent = nullptr);

    amnezia::ErrorCode extractConfigFromContainer(amnezia::DockerContainer container, const amnezia::ServerCredentials &credentials,
                                                  SshSession *sshSession, amnezia::ContainerConfig &config) override;
};

#endif // DNSTTINSTALLER_H
