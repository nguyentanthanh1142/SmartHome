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

// EMA Filter State cho Gas
static float emaGasValue = 0.0;
const float EMA_ALPHA = 0.2; // Hệ số lọc nhiễu (0.0 - 1.0)

// --- HÀM OVERSAMPLING CHUẨN CÔNG NGHIỆP ---
// Đọc ADC nhiều lần và lấy trung bình để loại bỏ nhiễu điện áp (ADC Noise)
static int readADC_Oversampled(int pin, int samples = 16) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100); // Chờ tụ ADC hồi phục
  }
  return sum / samples;
}

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
  
  // Lấy mẫu oversampling lúc boot để làm baseline chuẩn
  int initialReading = readADC_Oversampled(PIN_MQ2, 32); 
  gasBaseline = initialReading;
  gasThreshold = gasBaseline + GAS_MARGIN;
  emaGasValue = gasBaseline; // Khởi tạo EMA
  
  lcd.setCursor(0, 1);
  lcd.printf("Base:%d Thr:%d", gasBaseline, gasThreshold);
  Serial.println("[GAS] Base=" + String(gasBaseline) + " | Thr=" + String(gasThreshold));
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

// Trả về giá trị đã lọc EMA, không trả về raw ADC nữa
int readGasRaw() {
  int rawADC = readADC_Oversampled(PIN_MQ2, 16);
  
  if (emaGasValue == 0.0) emaGasValue = rawADC;
  emaGasValue = (EMA_ALPHA * rawADC) + ((1.0 - EMA_ALPHA) * emaGasValue);
  
  return (int)emaGasValue;
}

int readLightRaw() { 
  return readADC_Oversampled(PIN_LDR, 8); 
}

bool readFlame() { 
  return digitalRead(PIN_FLAME) == LOW; 
}

float readCurrentA() {
  // ACS712 cần oversampling cao hơn vì nhiễu dòng điện rất lớn
  int rawADC = readADC_Oversampled(PIN_ACS712, 32); 
  
  float acsVoltageAtPin = (rawADC / 4095.0) * 3.3;
  float acsVoltageAtSensor = acsVoltageAtPin * DIVIDER_RATIO;
  float currentA = (acsVoltageAtSensor - ACS712_ZERO_VOLTAGE) / ACS712_SENSITIVITY;

  // Chuẩn thực tế: Dòng điện âm là do offset, trả về 0.0
  return (currentA < 0.0) ? 0.0 : currentA; 
}

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