#!/bin/sh
#
# Starts nginx in a container as HTTP/2 and HTTP/3 reverse proxy in front of a
# backend that runs inside the smoketest process. The HTTP/2 and HTTP/3 tests of
# the client then talk to an independent HPACK and QPACK implementation.
#
#   smoketests/proxy/start-proxy.sh    - start the proxy
#   podman stop dekaf2-test-proxy      - stop it (or docker stop)
#
# Environment overrides (the smoketests read the ports and the proxy host as well):
#
#   DEKAF2_TEST_PROXY_PORT    - TCP and UDP port of the proxy on this host (default 8443)
#   DEKAF2_TEST_BACKEND_PORT  - port of the backend in the smoketests     (default 7660)
#   DEKAF2_TEST_BACKEND_HOST  - host of the backend, as seen from the proxy
#                               (default host.containers.internal, this host)
#   DEKAF2_TEST_PROXY_NETWORK - a container network to start the proxy in, instead of
#                               publishing its port on this host. The smoketests then
#                               run in a container of the same network, reach the proxy
#                               as dekaf2-test-proxy:8443 (DEKAF2_TEST_PROXY_HOST), and
#                               the backend host is the name of their container. That
#                               container has to run before the proxy starts, as nginx
#                               resolves the backend host at its start.
#   DEKAF2_TEST_PROXY_IMAGE   - the nginx image
#   DEKAF2_CONTAINER_CLI      - podman or docker (default podman if installed, else docker)

set -e

# the physical path - with podman on macOS, /tmp of the virtual machine is not the
# /tmp of the host, while /private/tmp is shared
DIR=$(cd "$(dirname "$0")" && pwd -P)
PORT=${DEKAF2_TEST_PROXY_PORT:-8443}
BACKEND_PORT=${DEKAF2_TEST_BACKEND_PORT:-7660}
BACKEND_HOST=${DEKAF2_TEST_BACKEND_HOST:-host.containers.internal}
NETWORK=${DEKAF2_TEST_PROXY_NETWORK:-}
IMAGE=${DEKAF2_TEST_PROXY_IMAGE:-docker.io/library/nginx:1.31.6-alpine}
NAME=dekaf2-test-proxy

if [ -n "$DEKAF2_CONTAINER_CLI" ]; then
	CLI=$DEKAF2_CONTAINER_CLI
elif command -v podman >/dev/null 2>&1; then
	CLI=podman
elif command -v docker >/dev/null 2>&1; then
	CLI=docker
else
	echo "neither podman nor docker found" >&2
	exit 1
fi

if [ -n "$NETWORK" ]; then
	# the smoketests reach the proxy by its name in the network
	CONNECT="--network=$NETWORK"
else
	CONNECT="-p $PORT:8443/tcp -p $PORT:8443/udp"

	# nginx reaches the backend on this host as host.containers.internal. podman
	# provides that name itself, docker gets it as an alias of the host gateway
	# (docker 20.10 or later). A docker command that is podman's compatibility
	# wrapper needs no alias.
	if [ "$BACKEND_HOST" = "host.containers.internal" ] && ! "$CLI" --version 2>/dev/null | grep -qi podman; then
		CONNECT="$CONNECT --add-host=host.containers.internal:host-gateway"
	fi
fi

# a self-signed certificate - the tests do not verify it
CERTDIR=${TMPDIR:-/tmp}
CERTDIR=${CERTDIR%/}/$NAME
mkdir -p "$CERTDIR"
CERTDIR=$(cd "$CERTDIR" && pwd -P)
openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj /CN=localhost \
	-keyout "$CERTDIR/key.pem" -out "$CERTDIR/cert.pem" 2>/dev/null
chmod 644 "$CERTDIR/key.pem"

"$CLI" rm -f "$NAME" >/dev/null 2>&1 || true

# $CONNECT is split into its options on purpose
"$CLI" run --rm -d --name "$NAME" $CONNECT \
	-e BACKEND_HOST="$BACKEND_HOST" \
	-e BACKEND_PORT="$BACKEND_PORT" \
	-v "$DIR/nginx.conf.template:/etc/nginx/templates/default.conf.template:ro" \
	-v "$CERTDIR:/etc/nginx/certs:ro" \
	"$IMAGE"
