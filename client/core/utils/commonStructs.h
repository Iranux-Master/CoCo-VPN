#ifndef COMMONSTRUCTS_H
#define COMMONSTRUCTS_H

#include <QString>
#include "core/utils/routeModes.h"

namespace amnezia
{
    struct ServerCredentials
    {
        QString hostName;
        QString userName;
        QString secretData;
        int port = 22;
        // CoCo: SHA-256 fingerprint of the server's SSH host key (OpenSSH format, with or without the
        // "SHA256:" prefix). When set, the SSH client refuses a server that presents another key.
        // Empty keeps the upstream behaviour (no check), for servers added by hand.
        QString hostKeySha256;

        bool isValid() const
        {
            return !hostName.isEmpty() && !userName.isEmpty() && !secretData.isEmpty() && port > 0;
        }
    };

    struct InstalledAppInfo {
        QString appName;
        QString packageName;
        QString appPath;

        bool operator==(const InstalledAppInfo& other) const {
            if (!packageName.isEmpty()) {
                return packageName == other.packageName;
            } else {
                return appPath == other.appPath;
            }
        }
    };

    struct DnsSettings
    {
        QString primaryDns;
        QString secondaryDns;
    };

    struct SplitTunnelingSettings
    {
        bool isSitesSplitTunnelingEnabled;
        RouteMode routeMode;
    };

    struct ConnectionSettings
    {
        DnsSettings dns;
        bool isApiConfig;
        SplitTunnelingSettings splitTunneling;
    };

    struct ExportSettings
    {
        DnsSettings dns;
    };
}

#endif // COMMONSTRUCTS_H


