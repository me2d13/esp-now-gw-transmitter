#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Initialize ESP-NOW with error handling and auto-reboot on failure
// Returns true if initialization was successful
bool setupEspNow();

// Send a message to a specific peer via ESP-NOW
// macAddress: 12-character hex string (e.g., "ECFABC2FE867")
// messageObj: JSON object containing the message to send
void sendEspNowMessage(const char* macAddress, JsonObject messageObj);

// Get the number of registered peers
uint8_t getEspNowPeerCount();

// Callbacks (must be accessible for ESP-NOW library)
void onEspNowDataSent(uint8_t *mac_addr, uint8_t sendStatus);
void onEspNowDataReceived(unsigned char* mac, unsigned char *incomingData, unsigned char len);

#endif // ESPNOW_HANDLER_H
