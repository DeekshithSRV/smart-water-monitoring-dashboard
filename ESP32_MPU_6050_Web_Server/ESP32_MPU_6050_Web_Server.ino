#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const int MQTT_PORT = 1883;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String deviceID;
String statusTopic;
String tankTopic;
String ackTopic;
String lightTopic;
String fanTopic;
String pumpTopic;
String tankActionTopic;
String lightStatusTopic;
String fanStatusTopic;
String pumpStatusTopic;
String waterLevelTopic;

bool tankFull = false;
bool lightState = false;
bool fanState = false;
bool pumpState = false;

int selectedDevice = 0;
unsigned long lastHeartbeat = 0;

void publishAck(const char* message)
{
  mqttClient.publish(ackTopic.c_str(), message);
}

void publishStatus()
{
  mqttClient.publish(statusTopic.c_str(), "ONLINE", false);
}

void publishDeviceStates()
{
  mqttClient.publish(lightStatusTopic.c_str(), lightState ? "ON" : "OFF", true);
  mqttClient.publish(fanStatusTopic.c_str(), fanState ? "ON" : "OFF", true);
  mqttClient.publish(pumpStatusTopic.c_str(), pumpState ? "ON" : "OFF", true);
  mqttClient.publish(tankTopic.c_str(), tankFull ? "FULL" : "EMPTY", true);
  mqttClient.publish(waterLevelTopic.c_str(), tankFull ? "90" : "0", true);
}

void printMenu()
{
  Serial.println();
  Serial.println("===== MENU =====");
  Serial.println("1. LIGHT");
  Serial.println("2. FAN");
  Serial.println("3. PUMP");
  Serial.println("4. TANK");
  Serial.println("5. STATUS");
  Serial.println();
  Serial.println("Select Device:");
}

void setLight(bool enabled)
{
  lightState = enabled;
  mqttClient.publish(lightStatusTopic.c_str(), enabled ? "ON" : "OFF", true);
  publishAck(enabled ? "LIGHT ON" : "LIGHT OFF");
  Serial.println(enabled ? "LIGHT ON" : "LIGHT OFF");
}

void setFan(bool enabled)
{
  fanState = enabled;
  mqttClient.publish(fanStatusTopic.c_str(), enabled ? "ON" : "OFF", true);
  publishAck(enabled ? "FAN ON" : "FAN OFF");
  Serial.println(enabled ? "FAN ON" : "FAN OFF");
}

void setPump(bool enabled)
{
  if (enabled && tankFull)
  {
    pumpState = false;
    publishAck("PUMP BLOCKED - TANK FULL");
    Serial.println("ERROR: PUMP BLOCKED - TANK FULL");
    return;
  }

  pumpState = enabled;
  mqttClient.publish(pumpStatusTopic.c_str(), enabled ? "ON" : "OFF", true);
  publishAck(enabled ? "PUMP ON" : "PUMP OFF");
  Serial.println(enabled ? "PUMP ON" : "PUMP OFF");
}

void setTank(bool full)
{
  tankFull = full;

  if (tankFull)
  {
    pumpState = false;
    mqttClient.publish(tankTopic.c_str(), "FULL", true);
    mqttClient.publish(waterLevelTopic.c_str(), "90", true);
    mqttClient.publish(pumpStatusTopic.c_str(), "OFF", true);
    publishAck("PUMP OFF");
    publishAck("TANK FULL");
    Serial.println("TANK FULL -> PUMP FORCED OFF");
    return;
  }

  mqttClient.publish(tankTopic.c_str(), "EMPTY", true);
  mqttClient.publish(waterLevelTopic.c_str(), "0", true);
  publishAck("TANK EMPTY");
  Serial.println("TANK EMPTY");
}

void connectWiFi()
{
  Serial.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String msg = "";

  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  msg.trim();
  msg.toUpperCase();

  String t = String(topic);

  Serial.println();
  Serial.println("DASHBOARD COMMAND");
  Serial.println(t);
  Serial.println(msg);

  if (t == lightTopic)
  {
    setLight(msg == "ON");
    return;
  }

  if (t == fanTopic)
  {
    setFan(msg == "ON");
    return;
  }

  if (t == pumpTopic)
  {
    setPump(msg == "ON");
    return;
  }

  if (t == tankActionTopic)
  {
    setTank(msg == "FULL");
    return;
  }
}

void connectMQTT()
{
  while (!mqttClient.connected() && WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connecting MQTT...");

    if (mqttClient.connect(
          deviceID.c_str(),
          nullptr,
          nullptr,
          statusTopic.c_str(),
          1,
          true,
          "OFFLINE"
        ))
    {
      Serial.println("MQTT Connected");

      mqttClient.subscribe(lightTopic.c_str());
      mqttClient.subscribe(fanTopic.c_str());
      mqttClient.subscribe(pumpTopic.c_str());
      mqttClient.subscribe(tankActionTopic.c_str());

      Serial.println("Subscribed dashboard commands:");
      Serial.println(lightTopic);
      Serial.println(fanTopic);
      Serial.println(pumpTopic);
      Serial.println(tankActionTopic);

      publishStatus();
      publishDeviceStates();
      Serial.println("ONLINE Published");
    }
    else
    {
      Serial.print("MQTT Error RC=");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void printStatus()
{
  Serial.println();
  Serial.println("===== STATUS =====");
  Serial.print("WiFi : ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
  Serial.print("MQTT : ");
  Serial.println(mqttClient.connected() ? "ONLINE" : "OFFLINE");
  Serial.print("Tank : ");
  Serial.println(tankFull ? "FULL" : "EMPTY");
  Serial.print("Light : ");
  Serial.println(lightState ? "ON" : "OFF");
  Serial.print("Fan : ");
  Serial.println(fanState ? "ON" : "OFF");
  Serial.print("Pump : ");
  Serial.println(pumpState ? "ON" : "OFF");
}

void handleSerialInput(String input)
{
  input.trim();
  input.toUpperCase();

  input.replace("=", " ");
  input.replace(":", " ");
  input.replace("-", " ");

  if (input == "LIGHT ON" || input == "L ON")
  {
    setLight(true);
    printMenu();
    return;
  }

  if (input == "LIGHT OFF" || input == "L OFF")
  {
    setLight(false);
    printMenu();
    return;
  }

  if (input == "FAN ON" || input == "F ON")
  {
    setFan(true);
    printMenu();
    return;
  }

  if (input == "FAN OFF" || input == "F OFF")
  {
    setFan(false);
    printMenu();
    return;
  }

  if (input == "PUMP ON" || input == "P ON")
  {
    setPump(true);
    printMenu();
    return;
  }

  if (input == "PUMP OFF" || input == "P OFF")
  {
    setPump(false);
    printMenu();
    return;
  }

  if (input == "FULL" || input == "TANK FULL")
  {
    setTank(true);
    printMenu();
    return;
  }

  if (input == "EMPTY" || input == "TANK EMPTY")
  {
    setTank(false);
    printMenu();
    return;
  }

  if (input == "1")
  {
    selectedDevice = 1;
    Serial.println("LIGHT Selected");
    Serial.println("Type ON or OFF");
    return;
  }

  if (input == "2")
  {
    selectedDevice = 2;
    Serial.println("FAN Selected");
    Serial.println("Type ON or OFF");
    return;
  }

  if (input == "3")
  {
    selectedDevice = 3;
    Serial.println("PUMP Selected");
    Serial.println("Type ON or OFF");
    return;
  }

  if (input == "4")
  {
    selectedDevice = 4;
    Serial.println("TANK Selected");
    Serial.println("Type FULL or EMPTY");
    return;
  }

  if (input == "5")
  {
    printStatus();
    printMenu();
    return;
  }

  if (selectedDevice == 1)
  {
    if (input == "ON" || input == "OFF")
      setLight(input == "ON");

    printMenu();
    return;
  }

  if (selectedDevice == 2)
  {
    if (input == "ON" || input == "OFF")
      setFan(input == "ON");

    printMenu();
    return;
  }

  if (selectedDevice == 3)
  {
    if (input == "ON" || input == "OFF")
      setPump(input == "ON");

    printMenu();
    return;
  }

  if (selectedDevice == 4)
  {
    if (input == "FULL" || input == "EMPTY")
      setTank(input == "FULL");

    printMenu();
  }
}

void setup()
{
  Serial.begin(115200);
  connectWiFi();

  deviceID = WiFi.macAddress();
  statusTopic = "home/" + deviceID + "/device/status";
  tankTopic = "home/" + deviceID + "/tank/status";
  ackTopic = "home/" + deviceID + "/ack";
  lightTopic = "home/" + deviceID + "/actions/light";
  fanTopic = "home/" + deviceID + "/actions/fan";
  pumpTopic = "home/" + deviceID + "/actions/pump";
  tankActionTopic = "home/" + deviceID + "/actions/tank";
  lightStatusTopic = "home/" + deviceID + "/light/status";
  fanStatusTopic = "home/" + deviceID + "/fan/status";
  pumpStatusTopic = "home/" + deviceID + "/pump/status";
  waterLevelTopic = "home/" + deviceID + "/water/level";

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  connectMQTT();

  Serial.println();
  Serial.print("Device ID: ");
  Serial.println(deviceID);

  printMenu();
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected())
    connectMQTT();

  mqttClient.loop();

  if (millis() - lastHeartbeat > 5000)
  {
    publishStatus();
    lastHeartbeat = millis();
  }

  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');
    handleSerialInput(input);
  }
}
