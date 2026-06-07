import os
import socket
import time
from threading import Thread

from flask import Flask, jsonify, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt


BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OLD_TOPIC_PREFIX = "shaik/water_system"
MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.hivemq.com")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
APP_PORT = int(os.getenv("PORT", "5001"))
PUBLIC_URL = os.getenv("PUBLIC_URL", "")
TANK_FULL_LIMIT = 90
FILL_STEP = 2
DEVICE_TIMEOUT = 15

app = Flask(__name__, template_folder=os.path.join(BASE_DIR, "templates"))
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

latest_data = {
    "device_id": "",
    "water": 0,
    "tank": "UNKNOWN",
    "pump": "OFF",
    "light": "OFF",
    "fan": "OFF",
    "system": "OFFLINE",
    "error": "WAITING FOR DEVICE...",
}

last_seen = 0
mqtt_client = mqtt.Client()


def local_ip_addresses():
    addresses = []

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(("8.8.8.8", 80))
            addresses.append(sock.getsockname()[0])
    except OSError:
        pass

    try:
        hostname = socket.gethostname()
        for item in socket.getaddrinfo(hostname, None, socket.AF_INET):
            ip = item[4][0]
            if not ip.startswith("127."):
                addresses.append(ip)
    except OSError:
        pass

    unique = []
    for ip in addresses:
        if ip not in unique:
            unique.append(ip)

    return unique or ["127.0.0.1"]


def local_ip_address():
    return local_ip_addresses()[0]


def bonjour_url():
    name = socket.gethostname().split(".")[0]
    return f"http://{name}.local:{APP_PORT}" if name else ""


def water_level():
    try:
        return int(float(latest_data["water"]))
    except (TypeError, ValueError):
        return 0


def emit_field(field, value, topic):
    socketio.emit(
        "mqtt_data",
        {
            "field": field,
            "topic": topic,
            "value": value,
            "device_id": latest_data["device_id"],
        },
    )


def set_field(field, value, topic):
    latest_data[field] = value
    emit_field(field, value, topic)


def mark_device_seen(topic):
    global last_seen

    last_seen = time.time()

    if latest_data["system"] != "ONLINE":
        set_field("system", "ONLINE", topic)
        set_field("error", "ESP32 IS RECONNECTED", topic)


def esp32_online():
    return latest_data["system"] == "ONLINE" and last_seen and time.time() - last_seen <= DEVICE_TIMEOUT


def refresh_device_liveness():
    if last_seen and time.time() - last_seen <= DEVICE_TIMEOUT:
        return

    if latest_data["system"] != "OFFLINE":
        latest_data["system"] = "OFFLINE"
        latest_data["pump"] = "OFF"
        latest_data["error"] = "ESP32 IS OFFLINE"
        emit_field("system", "OFFLINE", "dashboard/system/status")
        emit_field("pump", "OFF", "dashboard/pump/status")
        emit_field("error", "ESP32 IS OFFLINE", "dashboard/system/error")


def reject_if_offline():
    refresh_device_liveness()

    if esp32_online():
        return None

    message = "ESP32 IS OFFLINE - CONTROL DISABLED"
    set_field("system", "OFFLINE", "dashboard/system/status")
    set_field("pump", "OFF", "dashboard/pump/status")
    set_field("error", message, "dashboard/system/error")

    return jsonify(
        {
            "success": False,
            "blocked": True,
            "offline": True,
            "error": message,
        }
    )


def parse_ack(value, topic):
    upper_value = value.strip().upper()

    if upper_value in ("TANK FULL", "FULL"):
        set_field("tank", "FULL", topic)
        set_field("water", TANK_FULL_LIMIT, topic)
        set_field("pump", "OFF", topic)
        return

    if upper_value in ("TANK EMPTY", "EMPTY"):
        set_field("tank", "EMPTY", topic)
        set_field("water", 0, topic)
        set_field("error", "NONE", topic)
        return

    for field in ("light", "fan", "pump"):
        prefix = field.upper()
        if upper_value in (f"{prefix} ON", f"{prefix} OFF"):
            set_field(field, upper_value.split()[-1], topic)
            return

    if "PUMP BLOCKED" in upper_value or "ERROR" in upper_value:
        set_field("error", upper_value, topic)
        return

    set_field("error", value, topic)


def normalize_message(topic, value, retained=False):
    parts = topic.split("/")

    if topic.startswith(f"{OLD_TOPIC_PREFIX}/"):
        suffix = topic.removeprefix(f"{OLD_TOPIC_PREFIX}/")
        if not retained and not suffix.endswith("/control"):
            mark_device_seen(topic)
        field_map = {
            "water/level": "water",
            "pump/status": "pump",
            "light/status": "light",
            "fan/status": "fan",
            "system/status": "system",
            "system/error": "error",
        }
        field = field_map.get(suffix)
        if field:
            set_field(field, value, topic)
        return

    if len(parts) < 3 or parts[0] != "home":
        return

    if not retained and parts[2] != "actions":
        mark_device_seen(topic)

    latest_data["device_id"] = parts[1]
    section = parts[2]
    name = parts[3] if len(parts) > 3 else ""

    if section == "device" and name == "status":
        status_value = value.upper()

        if retained and status_value == "ONLINE":
            set_field("system", "OFFLINE", topic)
            set_field("pump", "OFF", topic)
            set_field("error", "WAITING FOR LIVE ESP32 HEARTBEAT", topic)
            return

        set_field("system", status_value, topic)
        if status_value == "ONLINE":
            set_field("error", "ESP32 IS RECONNECTED", topic)
        elif status_value == "OFFLINE":
            set_field("pump", "OFF", topic)
            set_field("error", "ESP32 IS OFFLINE", topic)
        return

    if section == "tank" and name == "status":
        tank_state = value.upper()
        set_field("tank", tank_state, topic)
        if tank_state == "FULL":
            set_field("water", TANK_FULL_LIMIT, topic)
            set_field("pump", "OFF", topic)
        return

    if section == "ack":
        parse_ack(value, topic)
        return

    if section in ("actions", "status") and name in ("light", "fan", "pump"):
        set_field(name, value.upper(), topic)
        return

    if section == "actions" and name == "tank":
        tank_state = value.upper()
        if tank_state in ("FULL", "EMPTY"):
            set_field("tank", tank_state, topic)
            set_field("water", TANK_FULL_LIMIT if tank_state == "FULL" else 0, topic)
            if tank_state == "EMPTY":
                set_field("error", "NONE", topic)
            if tank_state == "FULL":
                set_field("pump", "OFF", topic)
        return

    if section in ("light", "fan", "pump") and name in ("status", "state"):
        set_field(section, value.upper(), topic)
        return

    if section in ("water", "tank") and name in ("level", "percentage"):
        set_field("water", value, topic)


def on_connect(client, userdata, flags, rc):
    print("MQTT Connected:", rc)

    topics = [
        "home/+/device/status",
        "home/+/tank/status",
        "home/+/ack",
        "home/+/+/status",
        "home/+/water/level",
        f"{OLD_TOPIC_PREFIX}/water/level",
        f"{OLD_TOPIC_PREFIX}/pump/status",
        f"{OLD_TOPIC_PREFIX}/light/status",
        f"{OLD_TOPIC_PREFIX}/fan/status",
        f"{OLD_TOPIC_PREFIX}/system/status",
        f"{OLD_TOPIC_PREFIX}/system/error",
    ]

    for topic in topics:
        client.subscribe(topic)
        print("Subscribed:", topic)


def on_message(client, userdata, msg):
    value = msg.payload.decode(errors="replace")

    retained = bool(getattr(msg, "retain", False))
    retained_label = " [retained]" if retained else ""
    print(f"{msg.topic} -> {value}{retained_label}")
    normalize_message(msg.topic, value, retained)


def watchdog():
    while True:
        refresh_device_liveness()
        time.sleep(5)


def publish_control(device, state):
    device_id = latest_data["device_id"]
    published_topics = []

    if device_id:
        topic = f"home/{device_id}/actions/{device}"
        mqtt_client.publish(topic, state)
        published_topics.append(topic)

    old_topic = f"{OLD_TOPIC_PREFIX}/{device}/control"
    mqtt_client.publish(old_topic, state)
    published_topics.append(old_topic)

    return published_topics


def fill_tank():
    while True:
        if latest_data["pump"] == "ON":
            level = min(TANK_FULL_LIMIT, water_level() + FILL_STEP)
            set_field("water", level, "dashboard/water/level")

            if level >= TANK_FULL_LIMIT:
                set_field("tank", "FULL", "dashboard/tank/status")
                set_field("pump", "OFF", "dashboard/pump/status")
                set_field("error", "TANK IS FULL - MANUAL PUMP ON OVERRIDDEN", "dashboard/system/error")
                publish_control("pump", "OFF")

        time.sleep(1)


def start_mqtt():
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect_async(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()


@app.route("/")
def home():
    return render_template("index.html")


@app.route("/status")
def status():
    refresh_device_liveness()
    return jsonify(latest_data)


@app.route("/share-info")
def share_info():
    lan_urls = [f"http://{ip}:{APP_PORT}" for ip in local_ip_addresses()]
    bonjour = bonjour_url()
    if bonjour and bonjour not in lan_urls:
        lan_urls.append(bonjour)

    return jsonify(
        {
            "lan_url": lan_urls[0],
            "lan_urls": lan_urls,
            "local_url": f"http://localhost:{APP_PORT}",
            "public_url": PUBLIC_URL,
            "mqtt_broker": MQTT_BROKER,
            "mqtt_port": MQTT_PORT,
        }
    )


@app.route("/tank/empty")
def empty_tank():
    offline_response = reject_if_offline()
    if offline_response:
        return offline_response

    published_topics = publish_control("tank", "EMPTY")
    set_field("tank", "EMPTY", "dashboard/tank/status")
    set_field("water", 0, "dashboard/water/level")
    set_field("pump", "OFF", "dashboard/pump/status")
    set_field("error", "NONE", "dashboard/system/error")

    return jsonify(
        {
            "success": True,
            "tank": "EMPTY",
            "water": 0,
            "topics": published_topics,
        }
    )


@app.route("/device/<device>/<state>")
def control(device, state):
    state = state.upper()

    offline_response = reject_if_offline()
    if offline_response:
        return offline_response

    if device == "pump" and state == "ON":
        if latest_data["tank"] == "FULL" or water_level() >= TANK_FULL_LIMIT:
            message = "TANK IS FULL - MANUAL PUMP ON OVERRIDDEN"
            set_field("pump", "OFF", "dashboard/pump/status")
            set_field("error", message, "dashboard/system/error")
            publish_control("pump", "OFF")

            return jsonify(
                {
                    "success": False,
                    "blocked": True,
                    "device": device,
                    "state": "OFF",
                    "error": message,
                }
            )

    published_topics = publish_control(device, state)

    if device in ("pump", "light", "fan"):
        set_field(device, state, f"dashboard/{device}/status")

    if device == "pump" and state == "ON":
        set_field("tank", "FILLING", "dashboard/tank/status")
        set_field("error", "NONE", "dashboard/system/error")

    print(f"Control Command -> {device}: {state}")

    return jsonify(
        {
            "success": True,
            "device": device,
            "state": state,
            "topics": published_topics,
        }
    )


if __name__ == "__main__":
    start_mqtt()
    Thread(target=watchdog, daemon=True).start()
    Thread(target=fill_tank, daemon=True).start()

    print("====================================")
    print(" SMART WATER MONITORING DASHBOARD")
    print("====================================")
    print(f"MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print("Dashboard URL:")
    print(f"http://localhost:{APP_PORT}")
    print("Team LAN URL:")
    print(f"http://{local_ip_address()}:{APP_PORT}")
    if PUBLIC_URL:
        print("Public URL:")
        print(PUBLIC_URL)
    print("Waiting for device...")
    print("====================================")

    socketio.run(
        app,
        host="0.0.0.0",
        port=APP_PORT,
        debug=False,
        use_reloader=False,
        allow_unsafe_werkzeug=True,
    )
