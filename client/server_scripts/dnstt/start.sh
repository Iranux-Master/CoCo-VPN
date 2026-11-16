#!/bin/bash

# This script is copied from CoCo to /opt/amnezia in the container and runs every time the container starts.

echo "Container startup"

# Only DNS from outside. The VLESS inbound listens on 127.0.0.1 and is reached only through the tunnel.
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -p icmp -j ACCEPT
iptables -A INPUT -p udp --dport 53 -j ACCEPT
iptables -P INPUT DROP

ip6tables -A INPUT -i lo -j ACCEPT
ip6tables -A INPUT -m state --state RELATED,ESTABLISHED -j ACCEPT
ip6tables -A INPUT -p ipv6-icmp -j ACCEPT
ip6tables -P INPUT DROP

killall -KILL xray dnstt-server 2>/dev/null

# Each daemon is started again if it exits.
if [ -f /opt/amnezia/dnstt/server.json ]; then
  (while true; do xray -config /opt/amnezia/dnstt/server.json; sleep 2; done) &
fi

if [ -s /opt/amnezia/dnstt/server.key ]; then
  (while true; do
    dnstt-server -udp :53 -mtu $DNSTT_MTU -privkey-file /opt/amnezia/dnstt/server.key \
      $DNSTT_DOMAIN 127.0.0.1:$DNSTT_FORWARD_PORT
    sleep 2
  done) &
fi

tail -f /dev/null
