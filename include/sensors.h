#pragma once
#include "DHTesp.h"
#include <LiquidCrystal_I2C.h>

// ============================================================================
// SENSORS.H - PERCEPTION LAYER: doc cam bien nhiet do/do am, khi gas,
// anh sang, dong dien, lua, PIR chuyen dong
// ============================================================================

extern TempAndHumidity data1, data2;
extern bool dhtOk;

extern int gasBaseline;
extern int gasThreshold;

extern unsigned long lastMotionPK;
extern unsigned long lastMotionCT;

void setupSensors();                        // pinMode + khoi tao DHT + boot time
void calibrateGasSensor(LiquidCrystal_I2C &lcd);
void readDHTIfDue();                        // doc DHT moi 5s, cap nhat data1/data2/dhtOk

int   readGasRaw();
int   readLightRaw();
bool  readFlame();
float readCurrentA();

bool readPirPK();   // co debounce + settle-time, tu in Serial khi doi trang thai
bool readPirCT();
