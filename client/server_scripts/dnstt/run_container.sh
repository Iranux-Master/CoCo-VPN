# DNS tunnel: publish UDP 53 on the address of the default route only (not 0.0.0.0), so systemd-resolved's stub on
# 127.0.0.53 keeps working and nothing on the host's DNS setup is changed. Behind cloud NAT this is the private
# address the provider forwards to, which is what DNS queries arrive on.
DNSTT_LISTEN_IP=$(ip -4 route get 1.1.1.1 2>/dev/null | awk '{for (i = 1; i <= NF; i++) if ($i == "src") {print $(i + 1); exit}}')
if [ -z "$DNSTT_LISTEN_IP" ]; then
  echo "DNS tunnel: could not find the server's network address (no IPv4 default route)." >&2
  exit 1
fi

# Another DNS server on UDP 53 (bind, dnsmasq, unbound, a dnstt set up outside CoCo) stops the install before anything
# is changed. systemd-resolved's stub (127.0.0.53/127.0.0.54) and this container's own earlier port are fine.
DNSTT_BUSY=$(sudo ss -H -lun 'sport = :53' 2>/dev/null | awk '{print $4}' | grep -v -e '^127\.0\.0\.5[34]:' -e '^\[::1\]:' -e '^127\.0\.0\.1:' || true)
if [ -n "$DNSTT_BUSY" ] && ! sudo docker ps --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
  echo "DNS tunnel: UDP port 53 is already used on this server ($DNSTT_BUSY). Stop the other DNS server and try again." >&2
  exit 1
fi

sudo docker run -d \
--log-driver none \
--restart always \
--cap-add=NET_ADMIN \
-p "$DNSTT_LISTEN_IP:53:53/udp" \
--name $CONTAINER_NAME $CONTAINER_NAME

sudo docker network connect amnezia-dns-net $CONTAINER_NAME
