#pragma once
#define MQTT_MAX_PACKET_SIZE 1024
#include <WiFi.h>
#include <PubSubClient.h>

// ============================================================================
// MQTT_HANDLER.H - NETWORK LAYER: WiFi, ket noi MQTT, auto-discovery cho
// Home Assistant, dieu khien relay + bao cao du lieu cam bien (change-based)
// ============================================================================

extern PubSubClient mqttClient;

extern bool relayStatePK, relayStateBEP, relayStateCT, relayStatePN, relayStateSAN;
extern bool pkManual, ctManual, sanManual;

void connectWiFi();
void setupMQTT();          // dat server/callback/buffer size + tao client ID
void reconnectMQTT();      // co cooldown 5s giua cac lan thu ket noi lai

void setRelay(int pin, bool on, bool &stateVar, const char *name);

// Chi gui du lieu MQTT khi thay doi vuot nguong (deadband) - tu quan ly timer 15s ben trong ham
void publishSensorData(int gasRaw, int lightRaw, float currentA, bool doorOpen,
                        bool pirPK, bool pirCT, bool alarmActive, bool fireAlarm);
