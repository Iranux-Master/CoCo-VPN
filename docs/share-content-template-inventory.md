# Share content template inventory

The inventory below follows `Proto`, `DockerContainer`, `ContainerUtils::isShareable`, and the server service settings pages. Container names are the installed server implementations; protocol names are the user-facing connection/service types.

## VPN access

| Template key | Current container(s) | Sharing behavior |
|---|---|---|
| `full` | Self-hosted admin server | Exports the existing administrator server configuration, including its management access. Keep restricted to trusted recipients. |
| `openvpn` | `OpenVpn` | Creates the existing per-protocol client. Supports the CoCo VPN package and native `.ovpn` export. |
| `wireguard` | `WireGuard` | Creates the existing per-protocol client. Supports the CoCo VPN package and native WireGuard config export. |
| `awg` | `Awg`, `Awg2` | Creates an AmneziaWG client; the package config and native client config use the same AWG guide. |
| `ikev2` | `Ipsec` | IKEv2/IPsec. The repository enables this connection only on Windows builds. |
| `xray` | `Xray` | XRay connection export. The protocol implementation accepts VLESS, VMess, and Trojan formats. |
| `shadowsocks` | `SSXray` | Active Shadowsocks-based container; shared through the CoCo VPN connection package. |

## Server services

These services do not use the VPN protocol client manager. Their guide action reads the details already shown by the service settings page and opens the normal share screen for HTML/TXT export.

| Template key | Current container | Details included in share flow |
|---|---|---|
| `tor` | `TorWebSite` | Onion website address. |
| `dns` | `Dns` | Server DNS address. |
| `sftp` | `Sftp` | Host, port, username, and password. |
| `socks5` | `Socks5Proxy` | Host, port, username, and password. |
| `mtproxy` | `MtProxy` | Generated Telegram proxy link and QR. |
| `telemt` | `Telemt` | Generated Telegram proxy link and QR. |
| `tproxy` | `TProxy` | Generated Telegram WEB proxy link and QR. |

`Cloak` and `ShadowSocks` are legacy container entries marked unsupported by `ContainerUtils::isUnsupportedContainer`; they are not active connection template kinds. `TorWebSite`, `Dns`, `Sftp`, `Socks5Proxy`, `MtProxy`, `Telemt`, and `TProxy` are marked non-shareable in the VPN client selector because they are service guides rather than VPN client accounts.

The old server/protocol client manager remains the source of truth for creating, renaming, listing, and revoking VPN clients. The removed `Sharing/accountGroups` storage was only used by the added multi-protocol account bundle and batch UI.

