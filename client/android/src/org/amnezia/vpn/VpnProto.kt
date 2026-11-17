package org.amnezia.vpn

import org.amnezia.vpn.protocol.Protocol
import org.amnezia.vpn.protocol.awg.Awg
import org.amnezia.vpn.protocol.openvpn.OpenVpn
import org.amnezia.vpn.protocol.wireguard.Wireguard
import org.amnezia.vpn.protocol.xray.Dnstt
import org.amnezia.vpn.protocol.xray.Xray

enum class VpnProto(
    val label: String,
    // Process suffix from AndroidManifest.xml (android:process). The full process name starts with
    // the application id, which CoCo changes from upstream's org.amnezia.vpn (see AmneziaVpnService.isRunning).
    val processName: String,
    val serviceClass: Class<out AmneziaVpnService>
) {
    WIREGUARD(
        "WireGuard",
        ":amneziaAwgService",
        AwgService::class.java
    ) {
        override fun createProtocol(): Protocol = Wireguard()
    },

    AWG(
        "AmneziaWG",
        ":amneziaAwgService",
        AwgService::class.java
    ) {
        override fun createProtocol(): Protocol = Awg()
    },

    OPENVPN(
        "OpenVPN",
        ":amneziaOpenVpnService",
        OpenVpnService::class.java
    ) {
        override fun createProtocol(): Protocol = OpenVpn()
    },

    XRAY(
        "XRay",
        ":amneziaXrayService",
        XrayService::class.java
    ) {
        override fun createProtocol(): Protocol = Xray.instance
    },

    SSXRAY(
        "SSXRay",
        ":amneziaXrayService",
        XrayService::class.java
    ) {
        override fun createProtocol(): Protocol = Xray.instance
    },

    // CoCo DNS tunnel: dnstt-client in front of XRay, in the XRay service process (docs/plans/D1-dns-tunnel.md)
    DNSTT(
        "DNS tunnel",
        ":amneziaXrayService",
        XrayService::class.java
    ) {
        override fun createProtocol(): Protocol = Dnstt.instance
    };

    private var _protocol: Protocol? = null
    val protocol: Protocol
        get() {
            if (_protocol == null) _protocol = createProtocol()
            return _protocol ?: throw AssertionError("Set to null by another thread")
        }

    protected abstract fun createProtocol(): Protocol

    companion object {
        fun get(protocolName: String): VpnProto = VpnProto.valueOf(protocolName.uppercase())
    }
}
