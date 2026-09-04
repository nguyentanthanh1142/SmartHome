/*
 * ============================================================================
 * DU AN: HE THONG NHA THONG MINH (SMART HOME) - ESP32 IoT GATEWAY
 * Kien truc: 4 Lop (Perception, Network, Edge Processing, Application)
 *
 * Du an dung PlatformIO (khong phai Arduino IDE), cau truc chuan:
 *   include/config.h              : hang so, chan GPIO, nguong, topic MQTT
 *   include/globals.h              : doi tuong dung chung (LCD)
 *   include/sensors.h + src/sensors.cpp        : PERCEPTION - doc cam bien
 *   include/mqtt_handler.h + src/mqtt_handler.cpp : NETWORK - WiFi, MQTT, Auto-Discovery, relay
 *   include/access_control.h + src/access_control.cpp : RFID, ma PIN, khoa cua, coi bao
 *   include/secrets.h              : (giu nguyen file cu cua ban) WIFI_SSID, MQTT_HOST,...
 *   src/main.cpp (file nay)        : chi con lop APPLICATION - setup()/loop() dieu phoi
 *
 * Toan bo cac luu y ky thuat quan trong (an toan phan cung, workaround cho
 * Wokwi, chuan bi production...) van con nguyen trong tung file .h/.cpp
 * tuong ung, khong bi mat khi tach file.
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "config.h"
#include "globals.h"
#include "sensors.h"
#include "access_control.h"
#include "mqtt_handler.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- Trang thai bao dong / den San (thuoc Application layer) ----------------
static bool gasAlarmLatched = false;
static bool prevAlarmActive = false;

static bool sanLightState = false;
static bool pendingSanState = false;
static unsigned long sanStateChangeStartTime = 0;
static bool sanWaitingConfirm = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n==================================================");
  Serial.println("  SMART HOME - ESP32 IoT Gateway - Khoi dong");
  Serial.println("==================================================");

  lcd.init(); lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Smart Home 2T");
  lcd.setCursor(0, 1); lcd.print("Booting...");
  delay(1000);

  analogSetAttenuation(ADC_11db);

  pinMode(PIN_RELAY_PK, OUTPUT); digitalWrite(PIN_RELAY_PK, RELAY_OFF);
  pinMode(PIN_RELAY_BEP, OUTPUT); digitalWrite(PIN_RELAY_BEP, RELAY_OFF);
  pinMode(PIN_RELAY_CT, OUTPUT); digitalWrite(PIN_RELAY_CT, RELAY_OFF);
  pinMode(PIN_RELAY_PN, OUTPUT); digitalWrite(PIN_RELAY_PN, RELAY_OFF);
  pinMode(PIN_RELAY_SAN, OUTPUT); digitalWrite(PIN_RELAY_SAN, RELAY_OFF);

  setupSensors();
  setupAccessControl();

  calibrateGasSensor(lcd);

  int lightBootRaw = readLightRaw();
  Serial.println("[LDR] Muc anh sang luc boot=" + String(lightBootRaw) +
                 " | Nguong BAT(toi)>" + String(LDR_ON_THRESHOLD) +
                 " | Nguong TAT(sang)<" + String(LDR_OFF_THRESHOLD));

  lcd.setCursor(0, 1); lcd.print("Connecting WiFi...");
  connectWiFi();
  setupMQTT();

  Serial.println("[SYSTEM] Hoan tat khoi dong! Bo qua nhieu PIR trong " + String(PIR_SETTLE_MS / 1000) + "s dau...");
  lcd.clear();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();
  }

  readDHTIfDue();

  lcd.setCursor(0, 0);
  if (dhtOk) {
    lcd.printf("T1:%2.0fC T2:%2.0fC", data1.temperature, data2.temperature);
  } else {
    lcd.print("DHT Read Error  ");
  }

  int gasRaw    = readGasRaw();
  int lightRaw  = readLightRaw();
  bool flame    = readFlame();
  bool doorOpen = isDoorSensorOpen();

  bool pirPK = readPirPK();
  bool pirCT = readPirCT();

  float currentA = readCurrentA();
  bool overCurrent = (currentA > CURRENT_THRESHOLD_A);

  // ---- Edge Processing: quyet dinh bao dong (co hysteresis chong nhap nhay) ----
  if (!gasAlarmLatched && gasRaw > gasThreshold) gasAlarmLatched = true;
  else if (gasAlarmLatched && gasRaw < (gasThreshold - GAS_HYSTERESIS)) gasAlarmLatched = false;

  bool gasAlarm = gasAlarmLatched;
  bool fireAlarm = (gasAlarm || flame);
  bool alarmActive = fireAlarm || overCurrent;

  lcd.setCursor(0, 1);
  if (fireAlarm) {
    int gasOffThreshold = gasThreshold - GAS_HYSTERESIS;
    lcd.printf("%s G:%4d>%4d", flame ? "FIRE" : "GAS ", gasRaw, gasOffThreshold);
    tone(PIN_BUZZER, 2000);
  } else if (overCurrent) {
    lcd.printf("OVERCURRENT:%.1f", currentA);
    tone(PIN_BUZZER, 2000);
  } else {
    lcd.printf("G:%-4d L:%-4d %s", gasRaw, lightRaw, doorOpen ? "OPEN" : "CLOSE");
    noTone(PIN_BUZZER);
  }

  // ---- Xu ly bao dong: bat het relay + mo khoa cua ----
  if (alarmActive) {
    if (!prevAlarmActive) {
      Serial.println("[ALARM] BAT DAU bao dong!");
      Serial.print("  -> THU PHAM: Gas="); Serial.print(gasRaw);
      Serial.print(" (Nguong="); Serial.print(gasThreshold); Serial.print(")");
      Serial.print(" | Lua="); Serial.print(flame ? "CO" : "KHONG");
      Serial.print(" | Dong dien="); Serial.println(currentA);
    }
    setRelay(PIN_RELAY_PK, true, relayStatePK, "PK");
    setRelay(PIN_RELAY_BEP, true, relayStateBEP, "BEP");
    setRelay(PIN_RELAY_CT, true, relayStateCT, "CT");
    setRelay(PIN_RELAY_PN, true, relayStatePN, "PN");
    setRelay(PIN_RELAY_SAN, true, relayStateSAN, "SAN");
    sanLightState = true;
    unlockDoor();
  } else {
    if (prevAlarmActive) {
      Serial.println("[ALARM] Da HET bao dong");
      setRelay(PIN_RELAY_BEP, false, relayStateBEP, "BEP");
      setRelay(PIN_RELAY_PN, false, relayStatePN, "PN");
      if (mqttClient.connected()) mqttClient.publish(TOPIC_ALARM, "NONE", true);

      pkManual = false; ctManual = false; sanManual = false;
      sanLightState = false; pendingSanState = false; sanWaitingConfirm = false;

      if (lightRaw > LDR_ON_THRESHOLD) {
        sanLightState = true;
        setRelay(PIN_RELAY_SAN, true, relayStateSAN, "San");
      } else {
        setRelay(PIN_RELAY_SAN, false, relayStateSAN, "San");
      }
    }

    // ---- Tu dong bat/tat den theo chuyen dong (neu khong o che do thu cong) ----
    if (pirPK) { lastMotionPK = millis(); pkManual = false; }
    if (pirCT) { lastMotionCT = millis(); ctManual = false; }

    if (!pkManual) setRelay(PIN_RELAY_PK, (millis() - lastMotionPK < MOTION_HOLD_MS), relayStatePK, "PK");
    if (!ctManual) setRelay(PIN_RELAY_CT, (millis() - lastMotionCT < MOTION_HOLD_MS), relayStateCT, "CT");

    // ---- Tu dong bat/tat den San theo anh sang (co xac nhan 3s chong nhap nhay) ----
    if (!sanManual) {
      bool targetState = sanLightState;
      if (lightRaw > LDR_ON_THRESHOLD) targetState = true;
      else if (lightRaw < LDR_OFF_THRESHOLD) targetState = false;

      if (targetState != sanLightState) {
        if (pendingSanState != targetState) {
          pendingSanState = targetState;
          sanStateChangeStartTime = millis();
          sanWaitingConfirm = true;
          Serial.println(String("[LDR] L=") + lightRaw + " - Bat dau dem 3s de xac nhan doi sang " + (targetState ? "BAT" : "TAT"));
        } else if (millis() - sanStateChangeStartTime >= LDR_CONFIRM_TIME_MS) {
          sanLightState = pendingSanState;
          sanWaitingConfirm = false;
          Serial.println(String("[LDR] Da xac nhan sau 3s - doi sang ") + (sanLightState ? "BAT" : "TAT"));
          setRelay(PIN_RELAY_SAN, sanLightState, relayStateSAN, "San");
        }
      } else {
        if (sanWaitingConfirm) Serial.println("[LDR] Anh sang tro lai vung on dinh - HUY yeu cau doi trang thai");
        pendingSanState = sanLightState;
        sanWaitingConfirm = false;
      }
    }
  }
  prevAlarmActive = alarmActive;

  checkDoorAutoLock();
  handleAccessControl();

  publishSensorData(gasRaw, lightRaw, currentA, doorOpen, pirPK, pirCT, alarmActive, fireAlarm);

  delay(500);
}
