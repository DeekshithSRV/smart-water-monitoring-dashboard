#!/usr/bin/env zsh
set -e

cd "$(dirname "$0")"

LOCAL_IP="$(ipconfig getifaddr en0 2>/dev/null || true)"

export MQTT_BROKER="${MQTT_BROKER:-${LOCAL_IP:-127.0.0.1}}"
export MQTT_PORT="${MQTT_PORT:-1883}"
export PORT="${PORT:-5001}"

if ! command -v ngrok >/dev/null 2>&1; then
  echo "ngrok is not installed."
  echo "Install it from https://ngrok.com/download, then run this script again."
  exit 1
fi

ngrok http "$PORT" > /tmp/water-dashboard-ngrok.log 2>&1 &
NGROK_PID=$!

cleanup() {
  kill "$NGROK_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Starting ngrok tunnel..."
sleep 3

PUBLIC_URL="$(curl -s http://127.0.0.1:4040/api/tunnels \
  | venv/bin/python -c 'import json,sys; data=json.load(sys.stdin); tunnels=data.get("tunnels", []); urls=[t.get("public_url", "") for t in tunnels if t.get("proto") == "https"]; print(urls[0] if urls else "")')"

if [ -z "$PUBLIC_URL" ]; then
  echo "Could not read ngrok public URL. Check /tmp/water-dashboard-ngrok.log"
  exit 1
fi

export PUBLIC_URL

echo "Public dashboard URL: $PUBLIC_URL"
echo "Share this URL with remote teammates."

exec venv/bin/python backend/app.py
