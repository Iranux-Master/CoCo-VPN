#include "dnsttUtils.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrl>

#include "core/utils/constants/protocolConstants.h"

namespace amnezia::dnstt
{
    QString normalizeTunnelDomain(const QString &input)
    {
        QString d = input.trimmed().toLower();
        const int scheme = d.indexOf(QLatin1String("://"));
        if (scheme >= 0) {
            d = d.mid(scheme + 3);
        }
        const int slash = d.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            d = d.left(slash);
        }
        while (d.endsWith(QLatin1Char('.'))) {
            d.chop(1);
        }
        return d;
    }

    bool isValidTunnelDomain(const QString &domain)
    {
        if (domain.isEmpty() || domain.size() > 253) {
            return false;
        }
        static const QRegularExpression label(QStringLiteral("^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$"));
        const QStringList labels = domain.split(QLatin1Char('.'));
        if (labels.size() < 3) {
            return false;
        }
        for (const QString &l : labels) {
            if (!label.match(l).hasMatch()) {
                return false;
            }
        }
        // The top-level domain is never all digits (rules out an IPv4 address).
        static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
        if (digits.match(labels.last()).hasMatch()) {
            return false;
        }
        // The name server label (<label>-ns) must still fit in 63 characters.
        return labels.first().size() + 3 <= 63;
    }

    QString nameServerHost(const QString &domain)
    {
        if (!isValidTunnelDomain(domain)) {
            return {};
        }
        const int dot = domain.indexOf(QLatin1Char('.'));
        return domain.left(dot) + QStringLiteral("-ns") + domain.mid(dot);
    }

    QStringList requiredRecords(const QString &domain, const QString &serverIp)
    {
        const QString ns = nameServerHost(domain);
        if (ns.isEmpty()) {
            return {};
        }
        return { QStringLiteral("NS  %1.  ->  %2.").arg(domain, ns), QStringLiteral("A  %1.  ->  %2").arg(ns, serverIp) };
    }

    QString normalizeMtu(const QString &mtu)
    {
        bool ok = false;
        const int value = mtu.trimmed().toInt(&ok);
        if (!ok || value < protocols::dnstt::minMtu || value > protocols::dnstt::maxMtu) {
            return QString::fromLatin1(protocols::dnstt::defaultMtu);
        }
        return QString::number(value);
    }

    bool isValidPublicKey(const QString &key)
    {
        static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
        return hex.match(key).hasMatch();
    }

    namespace
    {
        // "ip" or "ip:port" (IPv4) / "[ip]:port" (IPv6) -> ip and port; false when the host is not an IP address.
        bool splitHostPort(const QString &value, int defaultPort, QString &ip, int &port)
        {
            QString host = value;
            port = defaultPort;
            if (host.startsWith(QLatin1Char('['))) {
                const int end = host.indexOf(QLatin1Char(']'));
                if (end < 0) {
                    return false;
                }
                const QString rest = host.mid(end + 1);
                host = host.mid(1, end - 1);
                if (rest.startsWith(QLatin1Char(':'))) {
                    bool ok = false;
                    port = rest.mid(1).toInt(&ok);
                    if (!ok) {
                        return false;
                    }
                } else if (!rest.isEmpty()) {
                    return false;
                }
            } else if (host.count(QLatin1Char(':')) == 1) {
                bool ok = false;
                port = host.section(QLatin1Char(':'), 1).toInt(&ok);
                host = host.section(QLatin1Char(':'), 0, 0);
                if (!ok) {
                    return false;
                }
            }
            QHostAddress address;
            if (port < 1 || port > 65535 || !address.setAddress(host)) {
                return false;
            }
            ip = address.toString();
            return true;
        }

        QString joinHostPort(const QString &ip, int port)
        {
            return ip.contains(QLatin1Char(':')) ? QStringLiteral("[%1]:%2").arg(ip).arg(port)
                                                  : QStringLiteral("%1:%2").arg(ip).arg(port);
        }
    }

    Resolver parseResolver(const QString &spec)
    {
        Resolver r;
        const QString s = spec.trimmed();
        const int colon = s.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            return r;
        }
        const QString kind = s.left(colon).toLower();
        const QString value = s.mid(colon + 1);

        if (kind == QLatin1String("udp") || kind == QLatin1String("dot")) {
            QString ip;
            int port = 0;
            const bool udp = kind == QLatin1String("udp");
            if (!splitHostPort(value, udp ? 53 : 853, ip, port)) {
                return r;
            }
            r.transport = udp ? Resolver::Transport::Udp : Resolver::Transport::Dot;
            r.ip = ip;
            r.address = joinHostPort(ip, port);
        } else if (kind == QLatin1String("doh")) {
            const QUrl url(value, QUrl::StrictMode);
            QHostAddress address;
            if (!url.isValid() || url.scheme() != QLatin1String("https") || !address.setAddress(url.host())) {
                return r;
            }
            r.transport = Resolver::Transport::Doh;
            r.ip = address.toString();
            r.address = url.toString();
        }
        return r;
    }

    QString resolverToSpec(const Resolver &resolver)
    {
        switch (resolver.transport) {
        case Resolver::Transport::Udp: return QStringLiteral("udp:") + resolver.address;
        case Resolver::Transport::Dot: return QStringLiteral("dot:") + resolver.address;
        case Resolver::Transport::Doh: return QStringLiteral("doh:") + resolver.address;
        }
        return {};
    }

    QList<Resolver> defaultResolvers(const QStringList &systemDnsServers)
    {
        QList<Resolver> list;
        for (const QString &server : systemDnsServers) {
            const Resolver r = parseResolver(QStringLiteral("udp:") + server);
            if (r.isValid()) {
                list.append(r);
            }
        }
        list.append(parseResolver(QStringLiteral("doh:https://1.1.1.1/dns-query")));
        list.append(parseResolver(QStringLiteral("doh:https://8.8.8.8/dns-query")));
        return list;
    }

    QStringList clientArguments(const Resolver &resolver, const QString &publicKey, const QString &domain, int localPort)
    {
        QStringList args;
        switch (resolver.transport) {
        case Resolver::Transport::Udp: args << QStringLiteral("-udp"); break;
        case Resolver::Transport::Doh: args << QStringLiteral("-doh"); break;
        case Resolver::Transport::Dot: args << QStringLiteral("-dot"); break;
        }
        args << resolver.address << QStringLiteral("-pubkey") << publicKey << domain
             << QStringLiteral("127.0.0.1:%1").arg(localPort);
        return args;
    }

    QJsonObject xrayConfigForLocalPort(const QJsonObject &clientConfig, int localPort)
    {
        QJsonObject config = clientConfig;
        config.remove(QLatin1String(protocols::dnstt::block));

        QJsonArray outbounds = config.value(QLatin1String(protocols::xray::outbounds)).toArray();
        bool found = false;
        for (int i = 0; i < outbounds.size(); ++i) {
            QJsonObject outbound = outbounds.at(i).toObject();
            if (outbound.value(QLatin1String("protocol")).toString() != QLatin1String("vless")) {
                continue;
            }
            QJsonObject settings = outbound.value(QLatin1String(protocols::xray::settings)).toObject();
            QJsonArray vnext = settings.value(QLatin1String(protocols::xray::vnext)).toArray();
            if (vnext.isEmpty()) {
                continue;
            }
            QJsonObject server = vnext.at(0).toObject();
            server[QLatin1String(protocols::xray::address)] = QStringLiteral("127.0.0.1");
            server[QLatin1String(protocols::xray::port)] = localPort;
            vnext[0] = server;
            settings[QLatin1String(protocols::xray::vnext)] = vnext;
            outbound[QLatin1String(protocols::xray::settings)] = settings;
            outbounds[i] = outbound;
            found = true;
        }
        if (!found) {
            return {};
        }
        config[QLatin1String(protocols::xray::outbounds)] = outbounds;
        return config;
    }
}
