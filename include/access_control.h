#pragma once
#include <Arduino.h>

extern bool doorUnlocked;

void setupAccessControl();
void unlockDoor();
void lockDoor();
void soundBuzzer(int durationMs);
bool isDoorSensorOpen();
void checkDoorAutoLock();
void handleAccessControl();

void handleRFIDCommand(const String& command, const String& uid, const String& name, const String& role);