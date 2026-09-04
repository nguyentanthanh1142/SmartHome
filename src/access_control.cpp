#include "access_control.h"
#include "config.h"
#include "globals.h"
#include "mqtt_handler.h"
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

static Servo cuaServo;
static MFRC522 rfid(PIN_RFID_SS, -1);

static const int NUM_AUTHORIZED_CARDS = 2;
static byte authorizedUIDs[NUM_AUTHORIZED_CARDS][4] = {
    {0xDE, 0xAD, 0xBE, 0xEF},
    {0x12, 0x34, 0x56, 0x78}};

// Doi ten PIN_1/PIN_2 -> PIN_CODE_1/PIN_CODE_2 de khong nham voi PIN_xxx (chan GPIO) ben config.h
static const String PIN_CODE_1 = "1234";
static const String PIN_CODE_2 = "5678";
static String serialInput = "";

bool doorUnlocked = false;
static unsigned long unlockTime = 0;

static bool isAuthorizedCard(MFRC522::Uid &uid) {
  for (int i = 0; i < NUM_AUTHORIZED_CARDS; i++) {
    bool match = true;
    for (byte j = 0; j < 4 && j < uid.size; j++) {
      if (uid.uidByte[j] != authorizedUIDs[i][j]) { match = false; break; }
    }
    if (match) return true;
  }
  return false;
}

void setupAccessControl() {
  pinMode(PIN_REED, INPUT);   // Mach that: BAT BUOC them dien tro pull-up 10k tu chan 39 len 3.3V
  pinMode(PIN_BUZZER, OUTPUT);

  cuaServo.setPeriodHertz(50);
  cuaServo.attach(PIN_SERVO, 500, 2400);
  cuaServo.write(0);

  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  rfid.PCD_Init();
}

void unlockDoor() {
  cuaServo.write(90);
  if (!doorUnlocked) Serial.println("[DOOR] Mo khoa cua");
  doorUnlocked = true;
  unlockTime = millis();
}

void lockDoor() {
  cuaServo.write(0);
  if (doorUnlocked) Serial.println("[DOOR] Khoa cua lai");
  doorUnlocked = false;
}

void soundBuzzer(int durationMs) {
  tone(PIN_BUZZER, 2000);
  delay(durationMs);
  noTone(PIN_BUZZER);
}

bool isDoorSensorOpen() { return digitalRead(PIN_REED) == HIGH; }

void checkDoorAutoLock() {
  if (doorUnlocked && millis() - unlockTime > UNLOCK_DURATION_MS) {
    lockDoor();
  }
}

void handleAccessControl() {
  // --- 1. Ma PIN nhap qua Serial ---
  while (Serial.available() > 0) {
    char inChar = Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      if (serialInput.length() > 0) {
        Serial.println();
        Serial.println("[PIN] Da nhap ma: " + serialInput);
        if (serialInput == PIN_CODE_1 || serialInput == PIN_CODE_2) {
          Serial.println("[PIN] Ma dung! Mo khoa cua.");
          lcd.setCursor(0, 1); lcd.print("Access Granted! ");
          unlockDoor();
          if (mqttClient.connected()) mqttClient.publish(TOPIC_RFID_LOG, ("{\"pin\":\"" + serialInput + "\",\"status\":\"granted\"}").c_str());
        } else {
          Serial.println("[PIN] Ma sai! Tu choi.");
          lcd.setCursor(0, 1); lcd.print("Access Denied!  ");
          soundBuzzer(300);
          if (mqttClient.connected()) mqttClient.publish(TOPIC_RFID_LOG, ("{\"pin\":\"" + serialInput + "\",\"status\":\"denied\"}").c_str());
        }
        serialInput = "";
      }
    } else if (isDigit(inChar)) {
      serialInput += inChar;
      Serial.print("*");
    }
  }

  // --- 2. The RFID ---
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uidStr = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
      uidStr += String(rfid.uid.uidByte[i], HEX);
    }
    if (isAuthorizedCard(rfid.uid)) {
      Serial.println("[RFID] The HOP LE: " + uidStr);
      lcd.setCursor(0, 1); lcd.print("Access Granted!");
      unlockDoor();
      if (mqttClient.connected()) mqttClient.publish(TOPIC_RFID_LOG, ("{\"uid\":\"" + uidStr + "\",\"status\":\"granted\"}").c_str());
    } else {
      Serial.println("[RFID] The KHONG hop le: " + uidStr);
      lcd.setCursor(0, 1); lcd.print("Access Denied!  ");
      soundBuzzer(300);
      if (mqttClient.connected()) mqttClient.publish(TOPIC_RFID_LOG, ("{\"uid\":\"" + uidStr + "\",\"status\":\"denied\"}").c_str());
    }
    delay(800);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}
