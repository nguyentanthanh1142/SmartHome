#include "sensors.h"
#include "config.h"
#include <Arduino.h>

DHTesp dht_t1;
DHTesp dht_t2;

TempAndHumidity data1;
TempAndHumidity data2;
bool dhtOk = false;

int gasBaseline = 0;
int gasThreshold = 0;

unsigned long lastMotionPK = 0;
unsigned long lastMotionCT = 0;

static unsigned long lastDhtRead = 0;
static unsigned long bootTime = 0;

static bool prevPirPK = false;
static bool prevPirCT = false;
static int  pirPKHighCount = 0;
static int  pirCTHighCount = 0;

void setupSensors() {
  pinMode(PIN_PIR_PK, INPUT_PULLDOWN);
  pinMode(PIN_PIR_CT, INPUT_PULLDOWN);
  pinMode(PIN_FLAME, INPUT_PULLUP);

  dht_t1.setup(PIN_DHT_T1, DHTesp::DHT22);
  dht_t2.setup(PIN_DHT_T2, DHTesp::DHT22);
  delay(2000);

  bootTime = millis();
  lastMotionPK = millis() - MOTION_HOLD_MS - 1000;
  lastMotionCT = millis() - MOTION_HOLD_MS - 1000;
}

void calibrateGasSensor(LiquidCrystal_I2C &lcd) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Calibrating Gas...");
  long sum = 0;
  const int N = 15;
  for (int i = 0; i < N; i++) {
    sum += analogRead(PIN_MQ2);
    delay(100);
  }
  gasBaseline = sum / N;
  gasThreshold = gasBaseline + GAS_MARGIN;
  lcd.setCursor(0, 1);
  lcd.printf("Base:%d Thr:%d", gasBaseline, gasThreshold);
  Serial.println("[GAS] Bien do nen=" + String(gasBaseline) + " Nguong canh bao=" + String(gasThreshold) + " Nguong tat=" + String(gasThreshold - GAS_HYSTERESIS));
  delay(1500);
}

void readDHTIfDue() {
  if (millis() - lastDhtRead > DHT_READ_INTERVAL) {
    lastDhtRead = millis();
    TempAndHumidity r1 = dht_t1.getTempAndHumidity();
    TempAndHumidity r2 = dht_t2.getTempAndHumidity();
    dhtOk = (dht_t1.getStatus() == DHTesp::ERROR_NONE && dht_t2.getStatus() == DHTesp::ERROR_NONE);
    if (dhtOk) { data1 = r1; data2 = r2; }
  }
}

int readGasRaw()   { return analogRead(PIN_MQ2); }
int readLightRaw() { return analogRead(PIN_LDR); }
bool readFlame()   { return digitalRead(PIN_FLAME) == LOW; }

float readCurrentA() {
  int acsRaw = analogRead(PIN_ACS712);
  float acsVoltageAtPin = (acsRaw / 4095.0) * 3.3;
  float acsVoltageAtSensor = acsVoltageAtPin * DIVIDER_RATIO;
  float currentA = (acsVoltageAtSensor - ACS712_ZERO_VOLTAGE) / ACS712_SENSITIVITY;
  return fabs(currentA);
}

// Gom chung logic debounce PIR (truoc day lap lai y het cho PK va CT)
static bool debouncedPir(int pin, int &highCount, bool &prevState, const char *label) {
  bool settled = (millis() - bootTime > PIR_SETTLE_MS);
  bool raw = digitalRead(pin) == HIGH;
  highCount = raw ? min(highCount + 1, PIR_DEBOUNCE_COUNT) : 0;
  bool state = settled && (highCount >= PIR_DEBOUNCE_COUNT);

  if (state && !prevState) Serial.println(String("[MOTION] Co nguoi - ") + label);
  if (!state && prevState) Serial.println(String("[MOTION] Het chuyen dong - ") + label);
  prevState = state;
  return state;
}

bool readPirPK() { return debouncedPir(PIN_PIR_PK, pirPKHighCount, prevPirPK, "PK"); }
bool readPirCT() { return debouncedPir(PIN_PIR_CT, pirCTHighCount, prevPirCT, "CT"); }
