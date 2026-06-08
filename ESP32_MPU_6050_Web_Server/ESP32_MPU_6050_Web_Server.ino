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
int waterLevel = 0;

const int TANK_FULL_LIMIT = 90;
const int FILL_STEP = 2;

int selectedDevice = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastFillUpdate = 0;

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
  mqttClient.publish(tankTopic.c_str(), tankFull ? "FULL" : (waterLevel > 0 ? "FILLING" : "EMPTY"), true);
  mqttClient.publish(waterLevelTopic.c_str(), String(waterLevel).c_str(), true);
}

void printMenu()
{
  Serial.println();
  Serial.println("===== MENU =====");
  Serial.println("1. LIGHT  -> UP = ON, DOWN = OFF");
  Serial.println("2. FAN    -> LEFT = ON, RIGHT = OFF");
  Serial.println("3. PUMP   -> PUSH = ON, PULL = OFF");
  Serial.println("4. TANK   -> FULL or EMPTY");
  Serial.println("5. STATUS");
  Serial.println();
  Serial.println("Select Device 1/2/3/4/5, then enter the command:");
  Serial.println("Direct commands also work: UP, DOWN, LEFT, RIGHT, PUSH, PULL");
}

void setLight(bool enabled)
{
  lightState = enabled;
  mqttClient.publish(lightStatusTopic.c_str(), enabled ? "ON" : "OFF", true);
  publishAck(enabled ? "UP COMMAND ACTIVATED - LIGHT ON" : "DOWN COMMAND ACTIVATED - LIGHT OFF");
  Serial.println(enabled ? "UP COMMAND ACTIVATED -> LIGHT ON" : "DOWN COMMAND ACTIVATED -> LIGHT OFF");
}

void setFan(bool enabled)
{
  fanState = enabled;
  mqttClient.publish(fanStatusTopic.c_str(), enabled ? "ON" : "OFF", true);
  publishAck(enabled ? "LEFT COMMAND ACTIVATED - FAN ON" : "RIGHT COMMAND ACTIVATED - FAN OFF");
  Serial.println(enabled ? "LEFT COMMAND ACTIVATED -> FAN ON" : "RIGHT COMMAND ACTIVATED -> FAN OFF");
}

void setPump(bool enabled)
{
  if (enabled && (tankFull || waterLevel >= TANK_FULL_LIMIT))
  {
    pumpState = false;
    mqttClient.publish(pumpStatusTopic.c_str(), "OFF", true);
    publishAck("PUSH COMMAND BLOCKED - TANK FULL");
    Serial.println("ERROR: PUSH COMMAND BLOCKED - TANK FULL");
    return;
  }

  pumpState = enabled;
  if (enabled)
    tankFull = false;

  mqttClient.publish(pumpStatusTopic.c_str(), enabled ? "ON" : "OFF", true);
  mqttClient.publish(tankTopic.c_str(), enabled ? "FILLING" : (tankFull ? "FULL" : (waterLevel > 0 ? "PARTIAL" : "EMPTY")), true);
  publishAck(enabled ? "PUSH COMMAND ACTIVATED - PUMP ON" : "PULL COMMAND ACTIVATED - PUMP OFF");
  Serial.println(enabled ? "PUSH COMMAND ACTIVATED -> PUMP ON" : "PULL COMMAND ACTIVATED -> PUMP OFF");
}

void setTank(bool full)
{
  tankFull = full;

  if (tankFull)
  {
    pumpState = false;
    waterLevel = TANK_FULL_LIMIT;
    mqttClient.publish(tankTopic.c_str(), "FULL", true);
    mqttClient.publish(waterLevelTopic.c_str(), String(waterLevel).c_str(), true);
    mqttClient.publish(pumpStatusTopic.c_str(), "OFF", true);
    publishAck("PUMP OFF");
    publishAck("TANK FULL");
    Serial.println("TANK FULL -> PUMP FORCED OFF");
    return;
  }

  pumpState = false;
  waterLevel = 0;
  mqttClient.publish(tankTopic.c_str(), "EMPTY", true);
  mqttClient.publish(waterLevelTopic.c_str(), String(waterLevel).c_str(), true);
  mqttClient.publish(pumpStatusTopic.c_str(), "OFF", true);
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
    setLight(msg == "UP" || msg == "ON");
    return;
  }

  if (t == fanTopic)
  {
    setFan(msg == "LEFT" || msg == "ON");
    return;
  }

  if (t == pumpTopic)
  {
    setPump(msg == "PUSH" || msg == "ON");
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
  Serial.println(tankFull ? "FULL" : (waterLevel > 0 ? "FILLING/PARTIAL" : "EMPTY"));
  Serial.print("Water : ");
  Serial.print(waterLevel);
  Serial.println("%");
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

  if (input == "UP" || input == "LIGHT UP" || input == "LIGHT ON" || input == "L ON")
  {
    setLight(true);
    printMenu();
    return;
  }

  if (input == "DOWN" || input == "LIGHT DOWN" || input == "LIGHT OFF" || input == "L OFF")
  {
    setLight(false);
    printMenu();
    return;
  }

  if (input == "LEFT" || input == "FAN LEFT" || input == "FAN ON" || input == "F ON")
  {
    setFan(true);
    printMenu();
    return;
  }

  if (input == "RIGHT" || input == "FAN RIGHT" || input == "FAN OFF" || input == "F OFF")
  {
    setFan(false);
    printMenu();
    return;
  }

  if (input == "PUSH" || input == "PUMP PUSH" || input == "PUMP ON" || input == "P ON")
  {
    setPump(true);
    printMenu();
    return;
  }

  if (input == "PULL" || input == "PUMP PULL" || input == "PUMP OFF" || input == "P OFF")
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
    Serial.println("Type UP for LIGHT ON or DOWN for LIGHT OFF");
    return;
  }

  if (input == "2")
  {
    selectedDevice = 2;
    Serial.println("FAN Selected");
    Serial.println("Type LEFT for FAN ON or RIGHT for FAN OFF");
    return;
  }

  if (input == "3")
  {
    selectedDevice = 3;
    Serial.println("PUMP Selected");
    Serial.println("Type PUSH for PUMP ON or PULL for PUMP OFF");
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
    if (input == "UP" || input == "ON")
      setLight(true);
    else if (input == "DOWN" || input == "OFF")
      setLight(false);
    else
      Serial.println("Invalid LIGHT command. Use UP or DOWN.");

    printMenu();
    return;
  }

  if (selectedDevice == 2)
  {
    if (input == "LEFT" || input == "ON")
      setFan(true);
    else if (input == "RIGHT" || input == "OFF")
      setFan(false);
    else
      Serial.println("Invalid FAN command. Use LEFT or RIGHT.");

    printMenu();
    return;
  }

  if (selectedDevice == 3)
  {
    if (input == "PUSH" || input == "ON")
      setPump(true);
    else if (input == "PULL" || input == "OFF")
      setPump(false);
    else
      Serial.println("Invalid PUMP command. Use PUSH or PULL.");

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

void updateTankFilling()
{
  if (!pumpState)
    return;

  if (millis() - lastFillUpdate < 1000)
    return;

  lastFillUpdate = millis();
  waterLevel = min(TANK_FULL_LIMIT, waterLevel + FILL_STEP);
  mqttClient.publish(waterLevelTopic.c_str(), String(waterLevel).c_str(), true);

  if (waterLevel >= TANK_FULL_LIMIT)
  {
    tankFull = true;
    pumpState = false;
    mqttClient.publish(tankTopic.c_str(), "FULL", true);
    mqttClient.publish(pumpStatusTopic.c_str(), "OFF", true);
    publishAck("TANK FULL");
    publishAck("PUMP OFF");
    Serial.println("TANK FULL -> PUMP FORCED OFF");
    return;
  }

  mqttClient.publish(tankTopic.c_str(), "FILLING", true);
  Serial.print("WATER LEVEL: ");
  Serial.print(waterLevel);
  Serial.println("%");
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
  updateTankFilling();

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
