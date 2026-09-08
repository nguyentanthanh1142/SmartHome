#pragma once
// ============================================================================
// CONFIG.H - Cau hinh chan (pin), nguong (threshold) va hang so he thong
// ============================================================================

// ---------- PERCEPTION LAYER ----------
const int PIN_DHT_T1     = 15;
const int PIN_DHT_T2     = 16;
const int PIN_PIR_PK     = 13;
const int PIN_PIR_CT     = 27;
const int PIN_MQ2        = 35;
const int PIN_LDR        = 34;
const int PIN_ACS712     = 33;
const int PIN_FLAME      = 32;
const int PIN_REED       = 39;
const int PIN_RFID_SS    = 5;
const int PIN_RFID_SCK   = 18;
const int PIN_RFID_MISO  = 19;
const int PIN_RFID_MOSI  = 23;
const int PIN_SERVO      = 25;
const int PIN_BUZZER     = 4;
const int PIN_RELAY_PK   = 2;
const int PIN_RELAY_BEP  = 17;
const int PIN_RELAY_CT   = 26;
const int PIN_RELAY_PN   = 14;
const int PIN_RELAY_SAN  = 12;

#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// ---------- EDGE PROCESSING LAYER ----------
const int   GAS_MARGIN            = 600;
const int   GAS_HYSTERESIS        = 300;
const float CURRENT_THRESHOLD_A   = 10.0;
const float ACS712_SENSITIVITY    = 0.100;
const float ACS712_ZERO_VOLTAGE   = 1.65;
const float DIVIDER_RATIO         = 1.0;
const int LDR_ON_THRESHOLD        = 2000;
const int LDR_OFF_THRESHOLD       = 1000;
const unsigned long LDR_CONFIRM_TIME_MS = 3000;
const int PIR_DEBOUNCE_COUNT      = 2;

// ---------- TIMING ----------
const unsigned long MQTT_RECONNECT_COOLDOWN_MS = 5000;
const unsigned long UNLOCK_DURATION_MS          = 5000;
const unsigned long SENSOR_PUBLISH_INTERVAL     = 15000;
const unsigned long DHT_READ_INTERVAL           = 5000;
const unsigned long PIR_SETTLE_MS               = 15000;
const unsigned long MOTION_HOLD_MS              = 5000;

// ---------- NETWORK LAYER: MQTT TOPICS ----------
#define TOPIC_TEMP_HUM      "smarthome/sensor/temp_hum"
#define TOPIC_GAS           "smarthome/sensor/gas"
#define TOPIC_LIGHT         "smarthome/sensor/light"
#define TOPIC_MOTION_PK     "smarthome/sensor/motion_pk"
#define TOPIC_MOTION_CT     "smarthome/sensor/motion_ct"
#define TOPIC_DOOR          "smarthome/sensor/door"
#define TOPIC_CURRENT       "smarthome/sensor/current"
#define TOPIC_ALARM         "smarthome/alarm"
#define TOPIC_RFID_LOG      "smarthome/rfid/log"
#define TOPIC_STATUS        "smarthome/status"
#define TOPIC_RELAY_STATE_PK  "smarthome/relay/pk/state"
#define TOPIC_RELAY_STATE_BEP "smarthome/relay/bep/state"
#define TOPIC_RELAY_STATE_CT  "smarthome/relay/ct/state"
#define TOPIC_RELAY_STATE_PN  "smarthome/relay/pn/state"
#define TOPIC_RELAY_STATE_SAN "smarthome/relay/san/state"
#define TOPIC_RELAY_CMD       "smarthome/relay/+/set"
#define TOPIC_LOCK_CMD        "smarthome/lock/set"
#define TOPIC_RFID_MANAGE     "smarthome/rfid/manage"