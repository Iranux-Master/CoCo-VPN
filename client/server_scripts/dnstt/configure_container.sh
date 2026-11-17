# DNS tunnel keys, made once and kept when the container is configured again, so shared accounts keep working.
mkdir -p /opt/amnezia/dnstt /opt/amnezia/xray

if [ ! -s /opt/amnezia/dnstt/server.key ] || [ ! -s /opt/amnezia/dnstt/server.pub ]; then
  (umask 077 && dnstt-server -gen-key -privkey-file /opt/amnezia/dnstt/server.key -pubkey-file /opt/amnezia/dnstt/server.pub >/dev/null)
fi
chmod 600 /opt/amnezia/dnstt/server.key

# The settings CoCo reads back when it rescans the server (DnsttInstaller::extractConfigFromContainer).
echo "$DNSTT_DOMAIN" > /opt/amnezia/dnstt/domain
echo "$DNSTT_MTU" > /opt/amnezia/dnstt/mtu

# The first account (VLESS id). CoCo writes /opt/amnezia/xray/server.json with it, the same way as for the XRay
# container (XrayConfigurator::writeServerConfigForSetup), with the inbound on 127.0.0.1:$DNSTT_FORWARD_PORT.
if [ ! -s /opt/amnezia/xray/xray_uuid.key ]; then
  xray uuid > /opt/amnezia/xray/xray_uuid.key
fi
