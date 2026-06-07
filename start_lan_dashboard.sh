#!/usr/bin/env zsh
set -e

cd "$(dirname "$0")"

LOCAL_IP="$(ipconfig getifaddr en0 2>/dev/null || true)"

export MQTT_BROKER="${MQTT_BROKER:-${LOCAL_IP:-127.0.0.1}}"
export MQTT_PORT="${MQTT_PORT:-1883}"
export PORT="${PORT:-5001}"

echo "Starting LAN dashboard on port $PORT"
echo "Team URL will be shown in the dashboard sharing panel."

exec venv/bin/python backend/app.py
