# DNS tunnel keys, made once and kept when the container is configured again, so shared accounts keep working.
mkdir -p /opt/amnezia/dnstt
cd /opt/amnezia/dnstt || exit 1

if [ ! -s /opt/amnezia/dnstt/server.key ] || [ ! -s /opt/amnezia/dnstt/server.pub ]; then
  (umask 077 && dnstt-server -gen-key -privkey-file /opt/amnezia/dnstt/server.key -pubkey-file /opt/amnezia/dnstt/server.pub >/dev/null)
fi
chmod 600 /opt/amnezia/dnstt/server.key

# The first account (VLESS id). CoCo writes server.json with it (the same way as the XRay container).
if [ ! -s /opt/amnezia/dnstt/xray_uuid.key ]; then
  xray uuid > /opt/amnezia/dnstt/xray_uuid.key
fi
