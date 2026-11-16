# CoCo DNS tunnel: dnstt-server (UDP 53) in front of a loopback-only XRay VLESS inbound.
# dnstt by David Fifield (CC0, https://www.bamsoftware.com/software/dnstt/), the same release as the webpack.ir
# catalog tool "dnstt". It is fetched through the Go module proxy, so go.sum and the Go checksum database verify it.
FROM golang:1.24-alpine AS dnstt-build
ARG DNSTT_MODULE="www.bamsoftware.com/git/dnstt.git"
ARG DNSTT_VERSION="v1.20260501.0"
RUN CGO_ENABLED=0 GOFLAGS=-trimpath go install "${DNSTT_MODULE}/dnstt-server@${DNSTT_VERSION}"

FROM alpine:3.20
LABEL maintainer="CoCo VPN"

ARG XRAY_RELEASE="v26.7.28"

RUN apk add --no-cache curl unzip bash openssl iptables ip6tables dumb-init && apk --update upgrade --no-cache

RUN mkdir -p /opt/amnezia/dnstt
RUN echo -e "#!/bin/bash\ntail -f /dev/null" > /opt/amnezia/start.sh && chmod a+x /opt/amnezia/start.sh

# The same XRay release as the XRay container (client/server_scripts/xray/Dockerfile).
RUN curl -fL https://github.com/XTLS/Xray-core/releases/download/${XRAY_RELEASE}/Xray-linux-64.zip -o /root/xray.zip && \
  unzip /root/xray.zip xray -d /usr/bin/ && chmod a+x /usr/bin/xray && rm /root/xray.zip

COPY --from=dnstt-build /go/bin/dnstt-server /usr/bin/dnstt-server

ENTRYPOINT [ "dumb-init", "/opt/amnezia/start.sh" ]
