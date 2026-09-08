#include "access_control.h"
#include "config.h"
#include "globals.h"
#include "mqtt_handler.h"
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

static Servo cuaServo;
static MFRC522 rfid(PIN_RFID_SS, -1);

struct AuthorizedUser
{
  String uid;
  String name;
  String role;
};

static std::vector<AuthorizedUser> authorizedUsers;
static const String RFID_USERS_FILE = "/rfid_users.json";

static const String PIN_CODE_1 = "1234";
static const String PIN_CODE_2 = "5678";
static String serialInput = "";

bool doorUnlocked = false;
static unsigned long unlockTime = 0;
static unsigned long lastRfidRead = 0;
const unsigned long RFID_COOLDOWN_MS = 2500;

const char *DEFAULT_USERS_JSON =
    "{\"users\":["
    "{\"uid\":\"01020304\",\"name\":\"Nguyen Van A\",\"role\":\"Owner\"},"
    "{\"uid\":\"11223344\",\"name\":\"Tran Thi B\",\"role\":\"Helper\"}"
    "]}";

static bool loadUsersFromFS()
{
  if (!LittleFS.exists(RFID_USERS_FILE))
  {
    Serial.println("[RFID-DB] File chua co, tu dong tao mac dinh...");
    File file = LittleFS.open(RFID_USERS_FILE, "w");
    if (file)
    {
      file.print(DEFAULT_USERS_JSON);
      file.close();
    }
  }

  File file = LittleFS.open(RFID_USERS_FILE, "r");
  if (!file)
    return false;

  String jsonStr = file.readString();
  file.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error)
  {
    Serial.println("[RFID-DB] Loi parse JSON: " + String(error.c_str()));
    return false;
  }

  authorizedUsers.clear();
  JsonArray users = doc["users"];
  for (JsonObject user : users)
  {
    AuthorizedUser u;
    u.uid = user["uid"].as<String>();
    u.name = user["name"].as<String>();
    u.role = user["role"].as<String>();
    authorizedUsers.push_back(u);
  }

  Serial.println("[RFID-DB] Da load " + String(authorizedUsers.size()) + " nguoi dung.");
  return true;
}

static bool saveUsersToFS()
{
  JsonDocument doc;
  JsonArray users = doc["users"].to<JsonArray>();

  for (const auto &u : authorizedUsers)
  {
    JsonObject user = users.add<JsonObject>();
    user["uid"] = u.uid;
    user["name"] = u.name;
    user["role"] = u.role;
  }

  File file = LittleFS.open(RFID_USERS_FILE, "w");
  if (!file)
  {
    Serial.println("[RFID-DB] Loi ghi file!");
    return false;
  }

  serializeJsonPretty(doc, file);
  file.close();

  Serial.println("[RFID-DB] Da luu " + String(authorizedUsers.size()) + " nguoi dung vao LittleFS");
  return true;
}

static int findUserIndex(const String &uid)
{
  // Dùng size_t để tránh cảnh báo so sánh signed/unsigned
  for (size_t i = 0; i < authorizedUsers.size(); i++)
  {
    if (authorizedUsers[i].uid == uid)
      return i;
  }
  return -1;
}

static String getUidString(MFRC522::Uid &uid)
{
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++)
  {
    if (uid.uidByte[i] < 0x10)
      uidStr += "0";
    uidStr += String(uid.uidByte[i], HEX);
  }
  return uidStr;
}

// =========================================================================
// 🛠️ OPTIMIZED: Dùng ArduinoJson thay cho snprintf, retain = false
// =========================================================================
static void publishAccessLog(const char *uidStr, const char *name, const char *role, const char *status)
{
  if (!mqttClient.connected())
    return;

  // Đóng gói JSON an toàn, tự động quản lý kích thước chuỗi
  JsonDocument doc;
  doc["uid"] = uidStr;
  doc["name"] = name;
  doc["role"] = role;
  doc["status"] = status;

  char payload[256]; // Buffer đủ lớn cho JSON an toàn
  serializeJson(doc, payload, sizeof(payload));

  // CRITICAL FIX: retain = false để tránh spam notification khi HA restart
  mqttClient.publish(TOPIC_RFID_LOG, payload, false);

  Serial.println("[MQTT] Published log: " + String(payload));
}
// =========================================================================

void setupAccessControl()
{
  pinMode(PIN_REED, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  cuaServo.setPeriodHertz(50);
  cuaServo.attach(PIN_SERVO, 500, 2400);
  cuaServo.write(0);

  if (!LittleFS.begin(true))
  {
    Serial.println("[RFID-DB] Loi khoi tao LittleFS!");
  }
  else
  {
    Serial.println("[RFID-DB] LittleFS OK");
    loadUsersFromFS();
  }

  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  rfid.PCD_Init();

  Serial.println("[ACCESS] RFID & Servo initialized.");
}

void unlockDoor()
{
  cuaServo.write(90);
  if (!doorUnlocked)
  {
    Serial.println("[DOOR] Mo khoa cua");
    lcd.setCursor(0, 1);
    lcd.print("Unlocked!       ");
  }
  doorUnlocked = true;
  unlockTime = millis();
}

void lockDoor()
{
  cuaServo.write(0);
  if (doorUnlocked)
  {
    Serial.println("[DOOR] Khoa cua lai");
    lcd.setCursor(0, 1);
    lcd.print("Locked.         ");
  }
  doorUnlocked = false;
}

void soundBuzzer(int durationMs)
{
  tone(PIN_BUZZER, 2000);
  delay(durationMs);
  noTone(PIN_BUZZER);
}

bool isDoorSensorOpen()
{
  return digitalRead(PIN_REED) == HIGH;
}

void checkDoorAutoLock()
{
  if (doorUnlocked && (millis() - unlockTime > UNLOCK_DURATION_MS))
  {
    lockDoor();
  }
}

void handleAccessControl()
{
  while (Serial.available() > 0)
  {
    char inChar = Serial.read();
    if (inChar == '\n' || inChar == '\r')
    {
      if (serialInput.length() > 0)
      {
        Serial.println();
        Serial.println("[PIN] Da nhap ma: " + serialInput);
        if (serialInput == PIN_CODE_1 || serialInput == PIN_CODE_2)
        {
          Serial.println("[PIN] Ma dung! Mo khoa cua.");
          unlockDoor();
          publishAccessLog("SerialPIN", "Admin", "System", "granted");
        }
        else
        {
          Serial.println("[PIN] Ma sai! Tu choi.");
          lcd.setCursor(0, 1);
          lcd.print("Access Denied!  ");
          soundBuzzer(300);
          publishAccessLog(serialInput.c_str(), "Unknown", "Intruder", "denied");
        }
        serialInput = "";
      }
    }
    else if (isDigit(inChar))
    {
      serialInput += inChar;
      Serial.print("*");
    }
  }

  if (millis() - lastRfidRead < RFID_COOLDOWN_MS)
    return;

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
  {
    lastRfidRead = millis();

    String uidStr = getUidString(rfid.uid);
    int userIndex = findUserIndex(uidStr);

    if (userIndex != -1)
    {
      Serial.println("[RFID] HOP LE: " + authorizedUsers[userIndex].name + " (" + authorizedUsers[userIndex].role + ")");
      unlockDoor();
      publishAccessLog(uidStr.c_str(), authorizedUsers[userIndex].name.c_str(), authorizedUsers[userIndex].role.c_str(), "granted");
    }
    else
    {
      Serial.println("[RFID] KHONG HOP LE: UID=" + uidStr);
      lcd.setCursor(0, 1);
      lcd.print("Access Denied!  ");
      soundBuzzer(300);
      publishAccessLog(uidStr.c_str(), "Unknown", "Intruder", "denied");
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}
void handleRFIDCommand(const String &command, const String &uid, const String &name, const String &role)
{
  if (command == "add")
  {
    if (findUserIndex(uid) != -1)
    {
      Serial.println("[RFID-DB] UID da ton tai: " + uid);
      publishAccessLog(uid.c_str(), name.c_str(), role.c_str(), "add_failed_exists");
      return;
    }
    AuthorizedUser u;
    u.uid = uid;
    u.name = name;
    u.role = role;
    authorizedUsers.push_back(u);

    if (saveUsersToFS())
    {
      Serial.println("[RFID-DB] Da them UID: " + uid);
      publishAccessLog(uid.c_str(), name.c_str(), role.c_str(), "added_via_mqtt");
    }
  }
  else if (command == "remove")
  {
    int idx = findUserIndex(uid);
    if (idx != -1)
    {
      String removedName = authorizedUsers[idx].name;
      String removedRole = authorizedUsers[idx].role;
      authorizedUsers.erase(authorizedUsers.begin() + idx);

      if (saveUsersToFS())
      {
        Serial.println("[RFID-DB] Da xoa UID: " + uid);
        publishAccessLog(uid.c_str(), removedName.c_str(), removedRole.c_str(), "removed_via_mqtt");
      }
    }
    else
    {
      publishAccessLog(uid.c_str(), "Unknown", "N/A", "remove_failed_not_found");
    }
  }
  // =========================================================================
  // PHẦN MỚI THÊM: Xử lý lệnh lấy danh sách
  // =========================================================================
  else if (command == "list")
  {
    Serial.println("[RFID-DB] Dang gui danh sach len MQTT...");

    JsonDocument doc;
    JsonArray users = doc["users"].to<JsonArray>();

    for (const auto &u : authorizedUsers)
    {
      JsonObject user = users.add<JsonObject>();
      user["uid"] = u.uid;
      user["name"] = u.name;
      user["role"] = u.role;
    }

    char payload[512];
    serializeJson(doc, payload, sizeof(payload));

    mqttClient.publish("smarthome/rfid/users", payload, false);
    Serial.println("[MQTT] Published user list.");
  }
}