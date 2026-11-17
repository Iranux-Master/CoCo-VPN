package org.amnezia.vpn.protocol.xray

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.VpnService.Builder
import java.io.File
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.net.URI
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread
import kotlinx.coroutines.delay
import org.amnezia.vpn.protocol.BadConfigException
import org.amnezia.vpn.protocol.Protocol
import org.amnezia.vpn.protocol.Statistics
import org.amnezia.vpn.protocol.VpnStartException
import org.amnezia.vpn.util.Log
import org.json.JSONArray
import org.json.JSONObject

private const val TAG = "Dnstt"
private const val CLIENT_TAG = "dnstt-client"

// Built by recipes/dnstt and copied into this module's jniLibs by client/cmake/android.cmake.
private const val CLIENT_LIBRARY = "libdnstt.so"
private const val LISTEN_TIMEOUT_MS = 10_000L
private const val LISTEN_PROBE_INTERVAL_MS = 200L

private const val SPLIT_TUNNEL_DISABLE = 0
private const val SPLIT_TUNNEL_INCLUDE = 1
private const val SPLIT_TUNNEL_EXCLUDE = 2

private val PUBLIC_KEY = Regex("^[0-9a-f]{64}$")

/**
 * CoCo DNS tunnel (docs/plans/D1-dns-tunnel.md), the Android counterpart of client/core/protocols/dnsttProtocol.cpp.
 *
 * dnstt-client runs as a child process and listens on 127.0.0.1:port; XRay (Xray.instance, same service process)
 * gets the account's VLESS config with its server pointed at that port. dnstt-client is a separate process, so
 * VpnService.protect cannot reach its sockets: CoCo's own app is left out of the VPN instead (the system does that
 * per app, which covers the child process), and the resolver is also passed to XRay as the excluded host.
 *
 * The rules match client/core/utils/dnstt/dnsttUtils.cpp: resolver specs "udp:ip[:port]", "dot:ip[:port]" and
 * "doh:https://ip/path", the system resolvers first, then DoH 1.1.1.1 and 8.8.8.8. Android uses IPv4 resolvers only,
 * because XRay excludes the host as a /32 route.
 */
class Dnstt : Protocol() {

    private data class Resolver(val flag: String, val address: String, val ip: String)

    private val xray: Xray get() = Xray.instance
    private var process: Process? = null
    @Volatile
    private var stopping = false

    override val statistics: Statistics
        get() = xray.statistics

    override fun internalInit() {
        xray.initialize(context, state, onError)
    }

    override suspend fun startVpn(config: JSONObject, vpnBuilder: Builder, protect: (Int) -> Boolean) {
        val configData = config.optJSONObject("dnstt_config_data")
            ?: throw BadConfigException("dnstt_config_data not found")
        val xrayJsonConfig = JSONObject(configData.optString("config"))
        val block = xrayJsonConfig.optJSONObject("dnstt") ?: throw BadConfigException("dnstt block not found")

        val domain = block.optString("domain").trim().trimEnd('.').lowercase()
        val publicKey = block.optString("pubkey").trim().lowercase()
        if (domain.isEmpty()) throw BadConfigException("DNS tunnel domain is missing")
        if (!PUBLIC_KEY.matches(publicKey)) throw BadConfigException("DNS tunnel public key is invalid")

        val resolver = resolvers(block.optJSONArray("resolvers")).firstOrNull()
            ?: throw VpnStartException("No usable DNS resolver for the DNS tunnel")

        val executable = File(context.applicationInfo.nativeLibraryDir, CLIENT_LIBRARY)
        if (!executable.canExecute()) {
            throw VpnStartException("DNS tunnel client is missing from this build: ${executable.path}")
        }

        val localPort = freeLocalPort()
        startClient(executable, resolver, publicKey, domain, localPort)
        try {
            waitUntilListening(localPort)
            xray.startVpn(innerConfig(config, xrayJsonConfig, resolver, localPort), vpnBuilder, protect)
        } catch (e: Exception) {
            stopClient()
            throw e
        }
    }

    override fun stopVpn() {
        xray.stopVpn()
        stopClient()
    }

    override fun reconnectVpn(vpnBuilder: Builder, protect: (Int) -> Boolean) {
        xray.reconnectVpn(vpnBuilder, protect)
    }

    private fun startClient(executable: File, resolver: Resolver, publicKey: String, domain: String, localPort: Int) {
        val command = listOf(
            executable.path,
            resolver.flag, resolver.address,
            "-pubkey", publicKey,
            domain,
            "127.0.0.1:$localPort"
        )
        Log.i(TAG, "Start dnstt-client: resolver ${resolver.flag} ${resolver.address}, local port $localPort")
        stopping = false
        val started = try {
            ProcessBuilder(command)
                .directory(context.cacheDir)
                .redirectErrorStream(true)
                .start()
        } catch (e: Exception) {
            throw VpnStartException("Failed to start the DNS tunnel client: ${e.message}")
        }
        process = started

        // dnstt-client logs sessions and errors (no secrets: the public key is not one).
        thread(name = "dnstt-client-log", isDaemon = true) {
            try {
                started.inputStream.bufferedReader().forEachLine { Log.i(CLIENT_TAG, it) }
            } catch (ignored: Exception) {
            }
        }
        thread(name = "dnstt-client-watch", isDaemon = true) {
            val exitCode = try {
                started.waitFor()
            } catch (ignored: InterruptedException) {
                return@thread
            }
            if (!stopping && process === started) {
                Log.e(TAG, "dnstt-client exited with code $exitCode")
                onError("DNS tunnel client stopped unexpectedly (exit code $exitCode)")
                stopVpn()
            }
        }
    }

    private fun stopClient() {
        stopping = true
        val running = process ?: return
        process = null
        running.destroy()
        try {
            if (!running.waitFor(2, TimeUnit.SECONDS)) running.destroyForcibly()
        } catch (ignored: InterruptedException) {
            running.destroyForcibly()
        }
    }

    private suspend fun waitUntilListening(port: Int) {
        val deadline = System.currentTimeMillis() + LISTEN_TIMEOUT_MS
        while (System.currentTimeMillis() < deadline) {
            val running = process ?: throw VpnStartException("DNS tunnel client stopped")
            if (!running.isAlive) {
                throw VpnStartException("DNS tunnel client exited with code ${running.exitValue()}")
            }
            try {
                Socket().use { it.connect(InetSocketAddress(InetAddress.getByName("127.0.0.1"), port), 200) }
                return
            } catch (ignored: Exception) {
                delay(LISTEN_PROBE_INTERVAL_MS)
            }
        }
        throw VpnStartException("DNS tunnel client did not start listening")
    }

    /** The account's XRay config, pointed at the local dnstt-client port, as an "xray" connection config. */
    private fun innerConfig(config: JSONObject, xrayJsonConfig: JSONObject, resolver: Resolver, localPort: Int): JSONObject {
        val xrayConfig = JSONObject(xrayJsonConfig.toString())
        xrayConfig.remove("dnstt")
        val outbounds = xrayConfig.optJSONArray("outbounds") ?: throw BadConfigException("outbounds not found")
        var found = false
        for (i in 0 until outbounds.length()) {
            val outbound = outbounds.optJSONObject(i) ?: continue
            if (outbound.optString("protocol") != "vless") continue
            val server = outbound.optJSONObject("settings")?.optJSONArray("vnext")?.optJSONObject(0) ?: continue
            server.put("address", "127.0.0.1")
            server.put("port", localPort)
            found = true
            break
        }
        if (!found) throw BadConfigException("vless outbound not found")

        val inner = JSONObject(config.toString())
        inner.remove("dnstt_config_data")
        inner.put("protocol", "xray")
        inner.put("xray_config_data", JSONObject().put("config", xrayConfig.toString()))
        inner.put("hostName", resolver.ip)
        leaveAppOutOfVpn(inner)
        return inner
    }

    /** dnstt-client's queries must not enter the tunnel they carry: keep CoCo's own app (its UID) out of the VPN. */
    private fun leaveAppOutOfVpn(config: JSONObject) {
        val ownPackage = context.packageName
        val apps = config.optJSONArray("splitTunnelApps") ?: JSONArray()
        val list = (0 until apps.length()).map { apps.optString(it) }.filter { it.isNotEmpty() }
        when (config.optInt("appSplitTunnelType")) {
            SPLIT_TUNNEL_INCLUDE -> {
                val others = list.filter { it != ownPackage }
                if (others.isEmpty()) {
                    // Only CoCo itself was listed: tunnel every other app instead.
                    config.put("appSplitTunnelType", SPLIT_TUNNEL_EXCLUDE)
                    config.put("splitTunnelApps", JSONArray().put(ownPackage))
                } else {
                    config.put("splitTunnelApps", JSONArray(others))
                }
            }
            SPLIT_TUNNEL_EXCLUDE -> {
                if (ownPackage !in list) config.put("splitTunnelApps", JSONArray(list + ownPackage))
            }
            else -> {
                config.put("appSplitTunnelType", SPLIT_TUNNEL_EXCLUDE)
                config.put("splitTunnelApps", JSONArray().put(ownPackage))
            }
        }
    }

    private fun resolvers(configured: JSONArray?): List<Resolver> {
        if (configured != null && configured.length() > 0) {
            val list = (0 until configured.length()).mapNotNull { parseResolver(configured.optString(it)) }
            if (list.isNotEmpty()) return list
            Log.w(TAG, "No configured resolver is usable, using the defaults")
        }
        return systemDnsServers().mapNotNull { parseResolver("udp:$it") } +
            listOfNotNull(
                parseResolver("doh:https://1.1.1.1/dns-query"),
                parseResolver("doh:https://8.8.8.8/dns-query")
            )
    }

    private fun parseResolver(spec: String): Resolver? {
        val s = spec.trim()
        val colon = s.indexOf(':')
        if (colon <= 0) return null
        val kind = s.substring(0, colon).lowercase()
        val value = s.substring(colon + 1)
        return when (kind) {
            "udp", "dot" -> {
                val parts = value.split(':')
                val ip = parts[0]
                val port = if (parts.size == 2) parts[1].toIntOrNull() else if (parts.size == 1) {
                    if (kind == "udp") 53 else 853
                } else null
                if (!isIpv4(ip) || port == null || port !in 1..65535) null
                else Resolver(if (kind == "udp") "-udp" else "-dot", "$ip:$port", ip)
            }
            "doh" -> {
                val uri = try { URI(value) } catch (ignored: Exception) { return null }
                val host = uri.host ?: return null
                if (uri.scheme != "https" || !isIpv4(host)) null else Resolver("-doh", value, host)
            }
            else -> null
        }
    }

    private fun systemDnsServers(): List<String> {
        val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager ?: return emptyList()
        val network = cm.activeNetwork ?: return emptyList()
        if (cm.getNetworkCapabilities(network)?.hasTransport(NetworkCapabilities.TRANSPORT_VPN) == true) {
            return emptyList()
        }
        return cm.getLinkProperties(network)?.dnsServers.orEmpty()
            .filterIsInstance<Inet4Address>()
            .mapNotNull { it.hostAddress }
    }

    private fun isIpv4(value: String): Boolean {
        val parts = value.split('.')
        return parts.size == 4 && parts.all { p -> p.isNotEmpty() && p.length <= 3 && p.all(Char::isDigit) && p.toInt() <= 255 }
    }

    private fun freeLocalPort(): Int =
        try {
            ServerSocket(0, 1, InetAddress.getByName("127.0.0.1")).use { it.localPort }
        } catch (e: Exception) {
            throw VpnStartException("Failed to find a free local port for the DNS tunnel: ${e.message}")
        }

    companion object {
        val instance: Dnstt by lazy { Dnstt() }
    }
}
