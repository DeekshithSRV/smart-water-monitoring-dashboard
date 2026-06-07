import paho.mqtt.client as mqtt

TOPIC_PREFIX = "shaik/water_system"

light = "OFF"
fan = "OFF"

def on_connect(client, userdata, flags, rc):

    client.subscribe(
        f"{TOPIC_PREFIX}/light/control"
    )

    client.subscribe(
        f"{TOPIC_PREFIX}/fan/control"
    )

    print("Device Simulator Connected")

def on_message(client, userdata, msg):

    global light
    global fan

    value = msg.payload.decode()

    if msg.topic == f"{TOPIC_PREFIX}/light/control":

        light = value

        client.publish(
            f"{TOPIC_PREFIX}/light/status",
            light
        )

        print("Light =",light)

    if msg.topic == f"{TOPIC_PREFIX}/fan/control":

        fan = value

        client.publish(
            f"{TOPIC_PREFIX}/fan/status",
            fan
        )

        print("Fan =",fan)

client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

client.connect(
    "broker.hivemq.com",
    1883,
    60
)

client.loop_forever()