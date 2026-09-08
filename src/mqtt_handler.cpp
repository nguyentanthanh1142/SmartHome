#include "mqtt_handler.h"
#include "config.h"
#include "secrets.h"
#include "access_control.h"
#include "sensors.h"
#include <Arduino.h>
#include <ArduinoJson.h> // <-- BẮT BUỘC THÊM ĐỂ PARSE JSON

static WiFiClient espClient;
PubSubClient mqttClient(espClient);
static String mqttClientId;
static unsigned long lastMqttAttempt = 0;

bool relayStatePK = false, relayStateBEP = false, relayStateCT = false, relayStatePN = false, relayStateSAN = false;
bool pkManual = false, ctManual = false, sanManual = false;

// ---------------- Auto-Discovery cho Home Assistant ----------------
static void sendDiscoveryConfig(const char *component, const char *object_id, String configJson) {
  if (!mqttClient.connected()) return;
  char topic[128];
  snprintf(topic, sizeof(topic), "homeassistant/%s/esp32_smarthome_v2/%s/config", component, object_id);

  String finalJson = "{\"unique_id\":\"esp32_smarthome_v2_" + String(object_id) + "\"," + configJson.substring(1);
  if (mqttClient.publish(topic, finalJson.c_str(), true)) {
    Serial.println(String("[DISCOVERY] Sent: ") + object_id);
  } else {
    Serial.println(String("[DISCOVERY] FAIL: ") + object_id);
  }
  delay(50);
}

static void setupMQTTDiscovery() {
  Serial.println("[MQTT] Dang gui cau hinh Auto-Discovery...");
  const char *deviceInfo = "\"device\":{\"identifiers\":[\"esp32_smarthome_v2\"],\"name\":\"ESP32 Smart Home Gateway\",\"manufacturer\":\"Espressif\",\"model\":\"ESP32 DevKit V4\",\"sw_version\":\"1.0.0\"}";

  sendDiscoveryConfig("sensor", "temp1", "{\"name\":\"Nhiệt độ Phòng Khách\",\"state_topic\":\"" TOPIC_TEMP_HUM "\",\"value_template\":\"{{ value_json.t1 }}\",\"unit_of_measurement\":\"°C\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("sensor", "hum1", "{\"name\":\"Độ ẩm Phòng Khách\",\"state_topic\":\"" TOPIC_TEMP_HUM "\",\"value_template\":\"{{ value_json.h1 }}\",\"unit_of_measurement\":\"%\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("sensor", "temp2", "{\"name\":\"Nhiệt độ Cầu thang\",\"state_topic\":\"" TOPIC_TEMP_HUM "\",\"value_template\":\"{{ value_json.t2 }}\",\"unit_of_measurement\":\"°C\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("sensor", "hum2", "{\"name\":\"Độ ẩm Cầu thang\",\"state_topic\":\"" TOPIC_TEMP_HUM "\",\"value_template\":\"{{ value_json.h2 }}\",\"unit_of_measurement\":\"%\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("sensor", "gas", "{\"name\":\"Nồng độ Gas\",\"state_topic\":\"" TOPIC_GAS "\",\"unit_of_measurement\":\"ppm\",\"device_class\":\"gas\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("sensor", "light", "{\"name\":\"Độ sáng môi trường\",\"state_topic\":\"" TOPIC_LIGHT "\",\"unit_of_measurement\":\"lx\",\"device_class\":\"illuminance\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("sensor", "current", "{\"name\":\"Dòng điện tiêu thụ\",\"state_topic\":\"" TOPIC_CURRENT "\",\"unit_of_measurement\":\"A\",\"device_class\":\"current\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("binary_sensor", "door", "{\"name\":\"Trạng thái Cửa\",\"state_topic\":\"" TOPIC_DOOR "\",\"payload_on\":\"OPEN\",\"payload_off\":\"CLOSED\",\"device_class\":\"door\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("binary_sensor", "motion_pk", "{\"name\":\"Chuyển động Phòng Khách\",\"state_topic\":\"" TOPIC_MOTION_PK "\",\"payload_on\":\"1\",\"payload_off\":\"0\",\"device_class\":\"motion\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("binary_sensor", "motion_ct", "{\"name\":\"Chuyển động Cầu thang\",\"state_topic\":\"" TOPIC_MOTION_CT "\",\"payload_on\":\"1\",\"payload_off\":\"0\",\"device_class\":\"motion\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("binary_sensor", "alarm", "{\"name\":\"Trạng thái Báo động\",\"state_topic\":\"" TOPIC_ALARM "\",\"payload_on\":\"FIRE\",\"payload_off\":\"NONE\",\"device_class\":\"problem\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("switch", "relay_pk", "{\"name\":\"Đèn Phòng Khách\",\"state_topic\":\"" TOPIC_RELAY_STATE_PK "\",\"command_topic\":\"smarthome/relay/pk/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("switch", "relay_bep", "{\"name\":\"Đèn Bếp\",\"state_topic\":\"" TOPIC_RELAY_STATE_BEP "\",\"command_topic\":\"smarthome/relay/bep/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("switch", "relay_ct", "{\"name\":\"Đèn Cầu thang\",\"state_topic\":\"" TOPIC_RELAY_STATE_CT "\",\"command_topic\":\"smarthome/relay/ct/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("switch", "relay_pn", "{\"name\":\"Đèn Phòng Ngủ\",\"state_topic\":\"" TOPIC_RELAY_STATE_PN "\",\"command_topic\":\"smarthome/relay/pn/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"," + String(deviceInfo) + "}");
  sendDiscoveryConfig("switch", "relay_san", "{\"name\":\"Đèn Sân\",\"state_topic\":\"" TOPIC_RELAY_STATE_SAN "\",\"command_topic\":\"smarthome/relay/san/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"," + String(deviceInfo) + "}");
  
  // RFID Log Discovery
  sendDiscoveryConfig("sensor", "rfid_log", 
    "{\"name\":\"Lich su quet the RFID\","
    "\"state_topic\":\"" TOPIC_RFID_LOG "\","
    "\"value_template\":\"{{ value_json.status }}\","
    "\"json_attributes_topic\":\"" TOPIC_RFID_LOG "\","
    "\"icon\":\"mdi:card-account-details\"," + String(deviceInfo) + "}");

  Serial.println("[MQTT] Da gui hoan tat cau hinh Auto-Discovery!");
}

// ---------------- Dieu khien Relay ----------------
void setRelay(int pin, bool on, bool &stateVar, const char *name) {
  digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
  if (on != stateVar) {
    Serial.println("[RELAY] " + String(name) + (on ? " - BAT" : " - TAT"));
    stateVar = on;
    const char *topic = nullptr;
    if (pin == PIN_RELAY_PK) topic = TOPIC_RELAY_STATE_PK;
    else if (pin == PIN_RELAY_BEP) topic = TOPIC_RELAY_STATE_BEP;
    else if (pin == PIN_RELAY_CT) topic = TOPIC_RELAY_STATE_CT;
    else if (pin == PIN_RELAY_PN) topic = TOPIC_RELAY_STATE_PN;
    else if (pin == PIN_RELAY_SAN) topic = TOPIC_RELAY_STATE_SAN;

    if (topic && mqttClient.connected()) {
      const char *payload = on ? "ON" : "OFF";
      mqttClient.publish(topic, payload, true);
    }
  }
}

// ---------------- Nhan lenh tu MQTT ----------------
static void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String t = String(topic);

  Serial.println("\n[MQTT-CALLBACK] Topic: " + t + " | Payload: " + msg);

  if (t == TOPIC_LOCK_CMD) {
    if (msg == "OPEN") unlockDoor();
    else if (msg == "CLOSE") lockDoor();
    return;
  }

  // --- XỬ LÝ LỆNH QUẢN LÝ RFID (THÊM MỚI) ---
  if (t == TOPIC_RFID_MANAGE) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (error) {
      Serial.println("[MQTT-CALLBACK] Loi parse JSON: " + String(error.c_str()));
      return;
    }
    String command = doc["command"] | "";
    String uid = doc["uid"] | "";
    String name = doc["name"] | "";
    String role = doc["role"] | "";
    
    Serial.println("[MQTT-CALLBACK] RFID Manage -> Cmd: " + command + ", UID: " + uid);
    handleRFIDCommand(command, uid, name, role); // Gọi hàm từ access_control.cpp
    return;
  }

  int relayPin = -1;
  bool *stateVar = nullptr;
  const char *relayName = "";

  if (t.indexOf("relay/pk/") >= 0) { relayPin = PIN_RELAY_PK; pkManual = true; stateVar = &relayStatePK; relayName = "PK"; }
  else if (t.indexOf("relay/bep/") >= 0) { relayPin = PIN_RELAY_BEP; stateVar = &relayStateBEP; relayName = "BEP"; }
  else if (t.indexOf("relay/ct/") >= 0) { relayPin = PIN_RELAY_CT; ctManual = true; stateVar = &relayStateCT; relayName = "CT"; }
  else if (t.indexOf("relay/pn/") >= 0) { relayPin = PIN_RELAY_PN; stateVar = &relayStatePN; relayName = "PN"; }
  else if (t.indexOf("relay/san/") >= 0) { relayPin = PIN_RELAY_SAN; sanManual = true; stateVar = &relayStateSAN; relayName = "SAN"; }

  if (relayPin != -1) {
    setRelay(relayPin, (msg == "ON"), *stateVar, relayName);
  }
}

// ---------------- WiFi & MQTT ----------------
void connectWiFi() {
  Serial.print("[WIFI] Dang ket noi toi SSID: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("[WIFI] Da ket noi! IP: " + WiFi.localIP().toString());
  else Serial.println("[WIFI] KHONG ket noi duoc sau 10s.");
}

void setupMQTT() {
  mqttClientId = "esp32-smarthome-" + WiFi.macAddress();
  Serial.println("[MQTT] Client ID: " + mqttClientId);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected() || (millis() - lastMqttAttempt < MQTT_RECONNECT_COOLDOWN_MS)) return;
  
  lastMqttAttempt = millis();
  Serial.print("[MQTT] Dang ket noi toi broker: ");
  Serial.println(MQTT_HOST);

  bool ok = mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASS, TOPIC_STATUS, 1, true, "offline");

  if (ok) {
    Serial.println("[MQTT] Da ket noi broker thanh cong!");
    
    // AUTO BOOT STATE SYNC
    Serial.println("[MQTT] Auto-sync: Dang ghi de trang thai retained cu...");
    mqttClient.publish(TOPIC_ALARM, "NONE", true);
    mqttClient.publish(TOPIC_RELAY_STATE_PK, "OFF", true);
    mqttClient.publish(TOPIC_RELAY_STATE_BEP, "OFF", true);
    mqttClient.publish(TOPIC_RELAY_STATE_CT, "OFF", true);
    mqttClient.publish(TOPIC_RELAY_STATE_PN, "OFF", true);
    mqttClient.publish(TOPIC_RELAY_STATE_SAN, "OFF", true);
    mqttClient.publish(TOPIC_STATUS, "online", true);
    
    setupMQTTDiscovery();
    
    Serial.println("[MQTT] Subscribing to topics...");
    mqttClient.subscribe(TOPIC_RELAY_CMD);
    mqttClient.subscribe(TOPIC_LOCK_CMD);
    mqttClient.subscribe(TOPIC_RFID_MANAGE); // <-- ĐÃ SUBSCRIBE
  } else {
    Serial.println("[MQTT] LOI ket noi, ma loi=" + String(mqttClient.state()));
  }
}

// ---------------- Change-based reporting ----------------
void publishSensorData(int gasRaw, int lightRaw, float currentA, bool doorOpen, bool pirPK, bool pirCT, bool alarmActive, bool fireAlarm) {
  static unsigned long lastSensorPublish = 0;
  static float lastPubT1 = -99, lastPubH1 = -99, lastPubT2 = -99, lastPubH2 = -99;
  static int lastPubGas = -1, lastPubLight = -1;
  static float lastPubCurrent = -1.0;
  static bool lastPubDoor = false, lastPubMotionPK = false, lastPubMotionCT = false, lastPubAlarm = false;

  if (!mqttClient.connected() || millis() - lastSensorPublish <= SENSOR_PUBLISH_INTERVAL) return;
  lastSensorPublish = millis();

  if (dhtOk) {
    if (abs(data1.temperature - lastPubT1) > 0.5 || abs(data1.humidity - lastPubH1) > 2.0 || abs(data2.temperature - lastPubT2) > 0.5 || abs(data2.humidity - lastPubH2) > 2.0) {
      char buf[160];
      snprintf(buf, sizeof(buf), "{\"t1\":%.1f,\"h1\":%.1f,\"t2\":%.1f,\"h2\":%.1f}", data1.temperature, data1.humidity, data2.temperature, data2.humidity);
      mqttClient.publish(TOPIC_TEMP_HUM, buf);
      lastPubT1 = data1.temperature; lastPubH1 = data1.humidity; lastPubT2 = data2.temperature; lastPubH2 = data2.humidity;
    }
  }
  if (abs(gasRaw - lastPubGas) > 50) { char buf[16]; snprintf(buf, sizeof(buf), "%d", gasRaw); mqttClient.publish(TOPIC_GAS, buf); lastPubGas = gasRaw; }
  if (abs(lightRaw - lastPubLight) > 100) { char buf[16]; snprintf(buf, sizeof(buf), "%d", lightRaw); mqttClient.publish(TOPIC_LIGHT, buf); lastPubLight = lightRaw; }
  if (fabs(currentA - lastPubCurrent) > 0.2) { char buf[16]; snprintf(buf, sizeof(buf), "%.2f", currentA); mqttClient.publish(TOPIC_CURRENT, buf); lastPubCurrent = currentA; }
  
  if (doorOpen != lastPubDoor) { mqttClient.publish(TOPIC_DOOR, doorOpen ? "OPEN" : "CLOSED", true); lastPubDoor = doorOpen; }
  if (pirPK != lastPubMotionPK) { mqttClient.publish(TOPIC_MOTION_PK, pirPK ? "1" : "0"); lastPubMotionPK = pirPK; }
  if (pirCT != lastPubMotionCT) { mqttClient.publish(TOPIC_MOTION_CT, pirCT ? "1" : "0"); lastPubMotionCT = pirCT; }
  if (alarmActive != lastPubAlarm) { mqttClient.publish(TOPIC_ALARM, fireAlarm ? "FIRE" : (alarmActive ? "OVERCURRENT" : "NONE"), true); lastPubAlarm = alarmActive; }
  
  mqttClient.publish(TOPIC_STATUS, "online", true);
}