#ifndef DNSTTPROTOCOL_H
#define DNSTTPROTOCOL_H

#include <QPointer>
#include <QProcess>
#include <QScopedPointer>

#include "core/utils/dnstt/dnsttUtils.h"
#include "vpnProtocol.h"

class XrayProtocol;

// CoCo DNS tunnel on desktop (docs/plans/D1-dns-tunnel.md). It wraps the XRay client instead of changing it:
//   1. picks a DNS resolver and routes its address around the tunnel (so dnstt's own queries never enter the TUN),
//   2. starts dnstt-client (unprivileged) listening on a free 127.0.0.1 port,
//   3. starts an XrayProtocol whose VLESS outbound points at that port and whose "server" is the resolver, so the
//      kill switch and split tunnelling exempt the resolver,
//   4. relays the XRay protocol's state and errors.
class DnsttProtocol : public VpnProtocol
{
    Q_OBJECT
public:
    explicit DnsttProtocol(const QJsonObject &configuration, QObject *parent = nullptr);
    ~DnsttProtocol() override;

    ErrorCode start() override;
    void stop() override;

private:
    ErrorCode startDnsttClient(const amnezia::dnstt::Resolver &resolver, int localPort);
    void stopDnsttClient();
    bool addResolverRoute(const QString &ip);
    void removeResolverRoute();
    void onInnerStateChanged(Vpn::ConnectionState state);

    static QStringList systemDnsServers();
    static int freeLocalPort();

    QJsonObject m_clientConfig; // XRay client config with the "dnstt" block
    QString m_domain;
    QString m_publicKey;
    QList<amnezia::dnstt::Resolver> m_resolvers;

    QScopedPointer<QProcess> m_dnsttProcess;
    QScopedPointer<XrayProtocol> m_xray;
    QString m_routedResolverIp;
    bool m_stopping = false;
};

#endif // DNSTTPROTOCOL_H
