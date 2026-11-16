#ifndef DNSTTUTILS_H
#define DNSTTUTILS_H

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// CoCo DNS tunnel (docs/plans/D1-dns-tunnel.md): rules shared by the install wizard, the server scripts' variables
// and the device side. Pure functions, no Qt event loop, so they are unit-tested in client/tests.
namespace amnezia::dnstt
{
    // Lower-cases and trims what the person typed (also drops a trailing dot and a scheme such as "https://").
    QString normalizeTunnelDomain(const QString &input);

    // True for a subdomain whose parent is below the top-level domain, e.g. t.example.com (not example.com): the
    // name server record sits beside it (t-ns.example.com), and delegating example.com itself would hand the whole
    // domain to the server. Same rule as the webpack.ir catalog script "dnstt". At most 253 characters, labels of
    // 1 to 63 letters, digits and inner hyphens. Expects a normalized value.
    bool isValidTunnelDomain(const QString &domain);

    // The name server host for the tunnel domain: t.example.com -> t-ns.example.com. Empty when invalid.
    QString nameServerHost(const QString &domain);

    // The two records the person adds at their DNS provider, as display lines:
    // "NS  t.example.com.  ->  t-ns.example.com." and "A  t-ns.example.com.  ->  <ip>".
    QStringList requiredRecords(const QString &domain, const QString &serverIp);

    // dnstt-server -mtu: a number from 512 to 1400, or the default when empty or out of range.
    QString normalizeMtu(const QString &mtu);

    // 64 lower-case hex digits.
    bool isValidPublicKey(const QString &key);

    // ── Device side ──────────────────────────────────────────────────────────────────────────────────────────────

    // A DNS resolver the tunnel's queries go through. Written in settings as "udp:192.0.2.1:53",
    // "doh:https://1.1.1.1/dns-query" or "dot:1.1.1.1:853". Only IP addresses are accepted, so the device can route
    // exactly that address around the tunnel before anything else resolves.
    struct Resolver {
        enum class Transport { Udp, Doh, Dot };
        Transport transport = Transport::Udp;
        QString address; // what dnstt-client gets: "ip:port" for UDP and DoT, the URL for DoH
        QString ip;      // the address to route outside the tunnel

        bool isValid() const { return !address.isEmpty() && !ip.isEmpty(); }
    };

    Resolver parseResolver(const QString &spec);
    QString resolverToSpec(const Resolver &resolver);

    // The order the device tries when the config names no resolver: the network's own DNS servers over UDP (in a
    // shutdown usually the only ones that still reach foreign name servers), then two public DoH resolvers.
    QList<Resolver> defaultResolvers(const QStringList &systemDnsServers);

    // dnstt-client arguments: "-udp ip:53 | -doh URL | -dot ip:853", "-pubkey KEY", DOMAIN, "127.0.0.1:PORT".
    QStringList clientArguments(const Resolver &resolver, const QString &publicKey, const QString &domain, int localPort);

    // The client config stored for a DNS tunnel account is an XRay client config plus a "dnstt" block. This returns
    // the XRay part for xray-core: the block removed and the VLESS outbound pointed at 127.0.0.1:localPort.
    // Empty when the config has no VLESS outbound.
    QJsonObject xrayConfigForLocalPort(const QJsonObject &clientConfig, int localPort);
}

#endif // DNSTTUTILS_H
