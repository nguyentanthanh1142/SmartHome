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

static unsigned long bootTime = 0;
static unsigned long lastLoopTime = 0;

void setup()
{
  // --- PRODUCTION STANDARD: BOOT GLITCH PREVENTION ---
  // Ghi trạng thái OFF (HIGH) TRƯỚC khi set pinMode OUTPUT
  // để tránh xung điện làm Relay hút trong giai đoạn Bootloader.
  digitalWrite(PIN_RELAY_PK, RELAY_OFF);
  pinMode(PIN_RELAY_PK, OUTPUT);
  digitalWrite(PIN_RELAY_BEP, RELAY_OFF);
  pinMode(PIN_RELAY_BEP, OUTPUT);
  digitalWrite(PIN_RELAY_CT, RELAY_OFF);
  pinMode(PIN_RELAY_CT, OUTPUT);
  digitalWrite(PIN_RELAY_PN, RELAY_OFF);
  pinMode(PIN_RELAY_PN, OUTPUT);
  digitalWrite(PIN_RELAY_SAN, RELAY_OFF);
  pinMode(PIN_RELAY_SAN, OUTPUT);

  Serial.begin(115200);
  delay(500);
  Serial.println("\n==================================================");
  Serial.println("  SMART HOME - ESP32 IoT Gateway - Khoi dong");
  Serial.println("==================================================");

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Home 2T");
  lcd.setCursor(0, 1);
  lcd.print("Booting...");
  delay(1000);

  analogSetAttenuation(ADC_11db);

  setupSensors();
  setupAccessControl();

  lcd.setCursor(0, 1);
  lcd.print("Gas warm-up...  ");
  delay(3000); // Bắt buộc warm-up MQ2 thật

  calibrateGasSensor(lcd);

  int lightBootRaw = readLightRaw();
  Serial.println("[LDR] Muc anh sang luc boot=" + String(lightBootRaw));

  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");
  connectWiFi();
  setupMQTT();

  Serial.println("[SYSTEM] Hoan tat khoi dong!");
  lcd.clear();
}
static int prevGasRaw = -1;
static int prevLightRaw = -1;

void loop()
{
  static unsigned long loopCount = 0;
  static unsigned long lastLogTime = 0;

  loopCount++;

  // Log mỗi 10 vòng loop để tránh spam
  if (millis() - lastLogTime > 5000)
  {
    Serial.println("\n--- LOOP STATUS ---");
    Serial.println("[LOOP] Count: " + String(loopCount));
    Serial.println("[LOOP] Uptime: " + String(millis() / 1000) + "s");
    Serial.println("[LOOP] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("[LOOP] WiFi Status: " + String(WiFi.status()));
    Serial.println("[LOOP] MQTT Connected: " + String(mqttClient.connected()));
    lastLogTime = millis();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!mqttClient.connected())
    {
      Serial.println("[MQTT] Disconnected! Attempting reconnect...");
      reconnectMQTT();
    }
    mqttClient.loop();
  }
  else
  {
    Serial.println("[WIFI] Disconnected! Status: " + String(WiFi.status()));
  }

  readDHTIfDue();
  lcd.setCursor(0, 0);
  if (dhtOk)
  {
    lcd.printf("T1:%2.0fC T2:%2.0fC", data1.temperature, data2.temperature);
    // Serial.println("[DHT] T1=" + String(data1.temperature) + "C H1=" + String(data1.humidity) +
    //                "% | T2=" + String(data2.temperature) + "C H2=" + String(data2.humidity) + "%");
  }
  else
  {
    lcd.print("DHT Read Error  ");
    Serial.println("[DHT] READ ERROR!");
  }

  int gasRaw = readGasRaw();
  int lightRaw = readLightRaw();
  bool flame = readFlame();
  bool doorOpen = isDoorSensorOpen();
  bool pirPK = readPirPK();
  bool pirCT = readPirCT();
  float currentA = readCurrentA();

  bool overCurrent = false;
  if (millis() - bootTime > 15000)
  {
    overCurrent = (currentA > CURRENT_THRESHOLD_A);
  }

  // int gasDelta = gasRaw - prevGasRaw;
  // if (abs(gasDelta) > 30)
  // {
  //   Serial.println("[SENSOR-CHANGE] Gas: " + String(prevGasRaw) + " -> " + String(gasRaw) + " (Delta: " + String(gasDelta) + ")");
  //   prevGasRaw = gasRaw;
  // }

  int lightDelta = lightRaw - prevLightRaw;
  if (abs(lightDelta) > 30)
  {
    Serial.println("[SENSOR-CHANGE] Light: " + String(prevLightRaw) + " -> " + String(lightRaw) + " (Delta: " + String(lightDelta) + ")");
    prevLightRaw = lightRaw;
  }

  // Serial.println("[SENSORS] Gas=" + String(gasRaw) + " Light=" + String(lightRaw) +
  //                " Flame=" + String(flame ? "ON" : "OFF") +
  //                " Door=" + String(doorOpen ? "OPEN" : "CLOSED"));
  // Serial.println("[SENSORS] PIR_PK=" + String(pirPK ? "ON" : "OFF") +
  //                " PIR_CT=" + String(pirCT ? "ON" : "OFF") +
  //                " Current=" + String(currentA) + "A");

  // bool overCurrent = (currentA > CURRENT_THRESHOLD_A);

  // Serial.println("[CHECK] OverCurrent=" + String(overCurrent ? "YES" : "NO") +
  //                " (threshold=" + String(CURRENT_THRESHOLD_A) + "A)");

  if (overCurrent)
    Serial.println("[CHECK] OverCurrent=YES (threshold=" + String(CURRENT_THRESHOLD_A) + "A)"); // <-- Thay thế bằng dòng này

  // ---- Edge Processing: quyet dinh bao dong ----
  if (!gasAlarmLatched && gasRaw > gasThreshold)
  {
    gasAlarmLatched = true;
    Serial.println("[ALARM-LATCH] Gas alarm triggered! Raw=" + String(gasRaw) + " > Threshold=" + String(gasThreshold));
  }
  else if (gasAlarmLatched && gasRaw < (gasThreshold - GAS_HYSTERESIS))
  {
    gasAlarmLatched = false;
    Serial.println("[ALARM-UNLATCH] Gas alarm cleared! Raw=" + String(gasRaw) + " < " + String(gasThreshold - GAS_HYSTERESIS));
  }

  bool gasAlarm = gasAlarmLatched;
  bool fireAlarm = (gasAlarm || flame);
  bool alarmActive = fireAlarm || overCurrent;

  if (alarmActive)
  {
    Serial.println("[ALARM-CHECK] GasAlarm=" + String(gasAlarm ? "YES" : "NO") +
                   " FireAlarm=" + String(fireAlarm ? "YES" : "NO") + " AlarmActive=YES");
  }
  lcd.setCursor(0, 1);
  if (fireAlarm)
  {
    int gasOffThreshold = gasThreshold - GAS_HYSTERESIS;
    lcd.printf("%s G:%4d >%4d ", flame ? "FIRE " : "GAS  ", gasRaw, gasOffThreshold);
    tone(PIN_BUZZER, 2000);
    Serial.println("[ALARM-ACTION] FIRE ALARM ACTIVE! Buzzer ON");
  }
  else if (overCurrent)
  {
    lcd.printf("OVERCURRENT:%.1f ", currentA);
    tone(PIN_BUZZER, 2000);
    Serial.println("[ALARM-ACTION] OVERCURRENT ALARM! Current=" + String(currentA) + "A");
  }
  else
  {
    lcd.printf("G:%-4d L:%-4d %s ", gasRaw, lightRaw, doorOpen ? "OPEN " : "CLOSE ");
    noTone(PIN_BUZZER);
  }

  // ---- Xu ly bao dong ----
  if (alarmActive)
  {
    if (!prevAlarmActive)
    {
      Serial.println("\n========== ALARM STARTED ==========");
      Serial.println("[ALARM] BAT DAU bao dong!");
      Serial.print("  -> THU PHAM: Gas=");
      Serial.print(gasRaw);
      Serial.print(" (Nguong=");
      Serial.print(gasThreshold);
      Serial.print(") ");
      Serial.print(" | Lua=");
      Serial.print(flame ? "CO" : "KHONG");
      Serial.print(" | Dong dien=");
      Serial.println(currentA);
    }
    Serial.println("[RELAY-CMD] Alarm mode: Turning ON all relays");
    setRelay(PIN_RELAY_PK, true, relayStatePK, "PK");
    setRelay(PIN_RELAY_BEP, true, relayStateBEP, "BEP");
    setRelay(PIN_RELAY_CT, true, relayStateCT, "CT");
    setRelay(PIN_RELAY_PN, true, relayStatePN, "PN");
    setRelay(PIN_RELAY_SAN, true, relayStateSAN, "SAN");
    sanLightState = true;
    unlockDoor();
    Serial.println("[DOOR] Unlock command sent");
  }
  else
  {
    if (prevAlarmActive)
    {
      Serial.println("\n========== ALARM CLEARED ==========");
      Serial.println("[ALARM] Da HET bao dong");
      Serial.println("[RELAY-CMD] Turning OFF BEP and PN relays");
      setRelay(PIN_RELAY_BEP, false, relayStateBEP, "BEP");
      setRelay(PIN_RELAY_PN, false, relayStatePN, "PN");
      if (mqttClient.connected())
      {
        Serial.println("[MQTT] Publishing ALARM=NONE");
        mqttClient.publish(TOPIC_ALARM, "NONE", true);
      }
      pkManual = false;
      ctManual = false;
      sanManual = false;
      sanLightState = false;
      pendingSanState = false;
      sanWaitingConfirm = false;
      if (lightRaw > LDR_ON_THRESHOLD)
      {
        Serial.println("[LDR-RECOVERY] Light raw=" + String(lightRaw) + " > " + String(LDR_ON_THRESHOLD) + " -> Turn ON San");
        sanLightState = true;
        setRelay(PIN_RELAY_SAN, true, relayStateSAN, "San");
      }
      else
      {
        Serial.println("[LDR-RECOVERY] Light raw=" + String(lightRaw) + " <= " + String(LDR_ON_THRESHOLD) + " -> Turn OFF San");
        setRelay(PIN_RELAY_SAN, false, relayStateSAN, "San");
      }
    }

    // ---- Tu dong bat/tat den theo chuyen dong ----
    if (pirPK)
    {
      lastMotionPK = millis();
      pkManual = false;
      Serial.println("[PIR-PK] Motion detected! Resetting timer");
    }
    if (pirCT)
    {
      lastMotionCT = millis();
      ctManual = false;
      Serial.println("[PIR-CT] Motion detected! Resetting timer");
    }

    bool pkState = (millis() - lastMotionPK < MOTION_HOLD_MS);
    bool ctState = (millis() - lastMotionCT < MOTION_HOLD_MS);

    if (!pkManual)
    {
      // Serial.println("[RELAY-PK] Auto mode: " + String(pkState ? "ON" : "OFF") +
      //                " (time since motion: " + String(millis() - lastMotionPK) + "ms)");
      setRelay(PIN_RELAY_PK, pkState, relayStatePK, "PK");
    }
    else
    {
      Serial.println("[RELAY-PK] Manual mode - skipping auto control");
    }

    if (!ctManual)
    {
      // Serial.println("[RELAY-CT] Auto mode: " + String(ctState ? "ON" : "OFF"));
      setRelay(PIN_RELAY_CT, ctState, relayStateCT, "CT");
    }
    else
    {
      Serial.println("[RELAY-CT] Manual mode - skipping auto control");
    }

    // ---- Tu dong bat/tat den San theo anh sang ----
    if (!sanManual)
    {
      bool targetState = sanLightState;
      if (lightRaw > LDR_ON_THRESHOLD)
      {
        targetState = true;
        Serial.println("[LDR] Light=" + String(lightRaw) + " > " + String(LDR_ON_THRESHOLD) + " -> Target ON");
      }
      else if (lightRaw < LDR_OFF_THRESHOLD)
      {
        targetState = false;
        Serial.println("[LDR] Light=" + String(lightRaw) + " < " + String(LDR_OFF_THRESHOLD) + " -> Target OFF");
      }

      if (targetState != sanLightState)
      {
        if (pendingSanState != targetState)
        {
          pendingSanState = targetState;
          sanStateChangeStartTime = millis();
          sanWaitingConfirm = true;
          Serial.println("[LDR] Starting 3s confirmation for state change to " + String(targetState ? "ON" : "OFF"));
        }
        else if (millis() - sanStateChangeStartTime >= LDR_CONFIRM_TIME_MS)
        {
          sanLightState = pendingSanState;
          sanWaitingConfirm = false;
          Serial.println("[LDR] Confirmed after 3s - Changing to " + String(sanLightState ? "ON" : "OFF"));
          setRelay(PIN_RELAY_SAN, sanLightState, relayStateSAN, "San");
        }
      }
      else
      {
        if (sanWaitingConfirm)
        {
          Serial.println("[LDR] Light stable - Canceling pending state change");
        }
        pendingSanState = sanLightState;
        sanWaitingConfirm = false;
      }
    }
    else
    {
      Serial.println("[RELAY-SAN] Manual mode - skipping LDR auto control");
    }
  }

  if (prevAlarmActive != alarmActive)
  {
    Serial.println("[STATE] Alarm state changed: " + String(alarmActive ? "ACTIVE" : "INACTIVE"));
  }
  prevAlarmActive = alarmActive;

  checkDoorAutoLock();
  handleAccessControl();

  // Serial.println("[MQTT] Publishing sensor data...");
  publishSensorData(gasRaw, lightRaw, currentA, doorOpen, pirPK, pirCT, alarmActive, fireAlarm);

  // Serial.println("--- END LOOP ---\n");
  if (millis() - lastLoopTime < 500)
    return;
  lastLoopTime = millis();
}