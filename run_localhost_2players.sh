#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_BIN="$ROOT_DIR/build/server_app"
CLIENT_BIN="$ROOT_DIR/build/client_app"

if [[ ! -x "$SERVER_BIN" ]]; then
  echo "Server binary not found: $SERVER_BIN" >&2
  exit 1
fi

if [[ ! -x "$CLIENT_BIN" ]]; then
  echo "Client binary not found: $CLIENT_BIN" >&2
  exit 1
fi

"$SERVER_BIN" &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT

sleep 1
echo "Spawning 2 players at different locations..."
"$CLIENT_BIN" 127.0.0.1 "Abdul" &
sleep 0.8
"$CLIENT_BIN" 127.0.0.1 "Kavyan" &

wait "$SERVER_PID"
