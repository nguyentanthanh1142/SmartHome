#pragma once
#include <Arduino.h>

// ============================================================================
// ACCESS_CONTROL.H - RFID, ma PIN qua Serial, khoa cua servo, coi bao (buzzer)
// ============================================================================

extern bool doorUnlocked;

void setupAccessControl();
void unlockDoor();
void lockDoor();
void soundBuzzer(int durationMs);

bool isDoorSensorOpen();          // doc cong tac tu (reed switch)
void checkDoorAutoLock();         // tu khoa cua lai sau UNLOCK_DURATION_MS
void handleAccessControl();       // xu ly RFID + ma PIN tu Serial - goi moi vong loop()
