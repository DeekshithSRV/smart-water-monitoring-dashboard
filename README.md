# Smart Water Monitoring Dashboard

Live ESP32 water-tank monitoring and control dashboard built with Flask, Socket.IO, MQTT, and Arduino ESP32.

## Features

- Live tank level display with animated filling.
- Pump, light, and fan dashboard controls.
- ESP32 online/offline detection.
- Pump protection when the tank is full or above 90%.
- Manual empty tank action.
- Dashboard siren for full tank warning.
- Team sharing through LAN or ngrok public tunnel.
- GitHub Pages live dashboard using MQTT over WebSocket.

## Project Structure

```text
backend/                         Flask dashboard backend
templates/                       Dashboard HTML, CSS, and JS
ESP32_MPU_6050_Web_Server/       Main Arduino IDE ESP32 sketch
esp32/                           Backup ESP32 sketch
start_lan_dashboard.sh           Start dashboard on same WiFi
start_public_tunnel.sh           Start dashboard with ngrok
```

## ESP32 Setup

The real WiFi password is stored locally in `secrets.h`, which is ignored by Git. For a new setup:

1. Copy `ESP32_MPU_6050_Web_Server/secrets.example.h` to `ESP32_MPU_6050_Web_Server/secrets.h`.
2. Update:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_SERVER = "YOUR_MQTT_BROKER_IP";
```

3. Open `ESP32_MPU_6050_Web_Server.ino` in Arduino IDE.
4. Select your ESP32 board and upload.

## Dashboard Setup

Install Python dependencies:

```bash
python3 -m venv venv
venv/bin/pip install -r backend/requirements.txt
```

Start same-WiFi dashboard:

```bash
./start_lan_dashboard.sh
```

Start public dashboard using ngrok:

```bash
./start_public_tunnel.sh
```


## GitHub Pages Live Dashboard

The `docs/` folder is a static dashboard for GitHub Pages. It connects directly to:

```text
wss://broker.hivemq.com:8884/mqtt
```

The ESP32 publishes to `broker.hivemq.com` on MQTT port `1883`. When the ESP32 connects to WiFi, the GitHub Pages dashboard receives the live data automatically.

Viewer mode can see live data only. Admin mode unlocks control buttons on the page.

Demo admin password:

```text
1234
```

For a college demo this is fine, but for real security use a backend with proper login because static GitHub Pages cannot fully protect secrets.

## MQTT Topics

The ESP32 publishes under:

```text
home/<device-id>/device/status
home/<device-id>/tank/status
home/<device-id>/water/level
home/<device-id>/light/status
home/<device-id>/fan/status
home/<device-id>/pump/status
home/<device-id>/ack
```

The dashboard sends commands to:

```text
home/<device-id>/actions/light
home/<device-id>/actions/fan
home/<device-id>/actions/pump
home/<device-id>/actions/tank
```
