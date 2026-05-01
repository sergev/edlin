#!/bin/sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EDLIN="$ROOT/edlin"
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

printf 'first line\nsecond line\n' >"$TMP"

printf 'L\n1L\nQ\nn\n' | "$EDLIN" "$TMP" >/dev/null

echo "smoke: ok"
