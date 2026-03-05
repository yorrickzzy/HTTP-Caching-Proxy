#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
COMPOSE_FILE="${ROOT_DIR}/docker-compose.yml"

PROXY_HOST="127.0.0.1"
PROXY_PORT="12345"
PROXY="http://${PROXY_HOST}:${PROXY_PORT}"
CHUNKED_URL="${CHUNKED_URL:-http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx}"
MAX_AGE_ZERO_URL="${MAX_AGE_ZERO_URL:-http://www.artsci.utoronto.ca/futurestudents}"
BAD_ORIGIN_HOST="${BAD_ORIGIN_HOST:-host.docker.internal}"
BAD_ORIGIN_PORT="${BAD_ORIGIN_PORT:-18081}"

ok(){ echo "ok"; }
fail(){ echo "fail: $1"; exit 1; }

need(){
  command -v "$1" >/dev/null 2>&1 || fail "Missing command: $1"
}

need curl
need nc
need grep
need docker

echo "[T0] docker proxy running..."
if ! sudo docker-compose -f "${COMPOSE_FILE}" ps --services --filter "status=running" | grep -qx "proxy"; then
  fail "Docker service 'proxy' is not running. Start it first: sudo docker-compose up -d --build"
fi
ok

echo "[T1] GET..."
curl -sS -x "${PROXY}" http://example.com >/dev/null || fail "GET via proxy failed"
ok

echo "[T2] chunked..."
CHUNKED_HEADERS=$(curl -sS -D - -o /dev/null -x "${PROXY}" "${CHUNKED_URL}" | tr -d '\r' || true)
echo "$CHUNKED_HEADERS" | grep -Eiq "^HTTP/" || fail "Failed to fetch chunked test URL"
ok

echo "[T3] max-age=0 (2 GETs)..."
curl -sS -x "${PROXY}" "${MAX_AGE_ZERO_URL}" >/dev/null || fail "GET max-age=0 target failed"
sleep 1
curl -sS -x "${PROXY}" "${MAX_AGE_ZERO_URL}" >/dev/null || fail "Second GET max-age=0 target failed"
ok

echo "[T4] bad CONNECT -> 400..."
RESP=$(printf "CONNECT badtarget HTTP/1.1\r\nHost: badtarget\r\n\r\n" | nc "${PROXY_HOST}" "${PROXY_PORT}" | head -n 1 || true)
echo "$RESP" | grep -q "400" || fail "Expected 400, got: $RESP"
ok

echo "[T5] bad method -> 400..."
RESP2=$(printf "FOO http://example.com/ HTTP/1.1\r\nHost: example.com\r\n\r\n" | nc "${PROXY_HOST}" "${PROXY_PORT}" | head -n 1 || true)
echo "$RESP2" | grep -q "400" || fail "Expected 400, got: $RESP2"
ok

echo "[T6] concurrency..."
seq 1 10 | xargs -n1 -P10 -I{} curl -sS -x "${PROXY}" http://example.com >/dev/null
ok

echo "[T7] POST..."
POST_OUT=$(curl -sS -x "${PROXY}" \
  -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  --data-urlencode "custname=1" \
  --data-urlencode "custtel=2" \
  --data-urlencode "custemail=3@2.com" \
  --data-urlencode "size=small" \
  --data-urlencode "topping=bacon" \
  --data-urlencode "delivery=12:00" \
  --data-urlencode "comments=2" \
  "http://httpbin.org/post" || true)
echo "$POST_OUT" | grep -q '"form": {' || fail "POST forwarding response missing form payload"
echo "$POST_OUT" | grep -q '"custemail": "3@2.com"' || fail "POST forwarding missing expected form field"
echo "$POST_OUT" | grep -q '"url": "http://httpbin.org/post"' || fail "POST forwarding reached unexpected target URL"
ok

echo "[T8] unreachable origin -> 502..."
RESP=$(curl -s -o /dev/null -w "%{http_code}" -x "${PROXY}" "http://no-such-domain.invalid/" || true)
[ "$RESP" = "502" ] || fail "Expected 502 for unreachable origin, got $RESP"

ok

echo "[T9] malformed origin response -> 502..."
T9_PATH="/t9-${RANDOM}-$(date +%s)"
T9_URL="http://${BAD_ORIGIN_HOST}:${BAD_ORIGIN_PORT}${T9_PATH}"
# Intentionally malformed status line.
(printf "BAD_STATUS_LINE\r\n\r\n" | nc -l -w 1 "${BAD_ORIGIN_PORT}" >/dev/null 2>&1) &
NC_PID=$!
sleep 0.5

RESP_BAD=$(curl --max-time 8 -s -o /dev/null -w "%{http_code}" -x "${PROXY}" \
  "${T9_URL}" || true)

kill "${NC_PID}" >/dev/null 2>&1 || true
wait "${NC_PID}" 2>/dev/null || true

[ "${RESP_BAD}" = "502" ] || fail "Expected 502 for malformed origin response, got ${RESP_BAD}"
ok
