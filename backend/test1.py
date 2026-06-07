import paho.mqtt.client as mqtt
import time



client = mqtt.Client()
client.connect("broker.hivemq.com",1883,60)

water = 20
pump = False

while True:

    if pump:
        water += 5
    else:
        water -= 2

    if water <= 20:
        pump = True

    if water >= 100:
        pump = False

    water = max(0,min(100,water))

    client.publish(
        f"{TOPIC_PREFIX}/water/level",
        str(water)
    )

    client.publish(
        f"{TOPIC_PREFIX}/pump/status",
        "ON" if pump else "OFF"
    )

    client.publish(
        f"{TOPIC_PREFIX}/system/status",
        "ONLINE"
    )

    print(
        f"Water={water}% Pump={'ON' if pump else 'OFF'}"
    )

    time.sleep(2)