#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_now.h>

// Initialize ESP-NOW with error handling and auto-reboot on failure
// Returns true if initialization was successful
bool setupEspNow();

// Send a message to a specific peer via ESP-NOW
// macAddress: 12-character hex string (e.g., "ECFABC2FE867")
// messageObj: JSON object containing the message to send
void sendEspNowMessage(const char* macAddress, JsonObject messageObj);

// Get the number of registered peers
uint8_t getEspNowPeerCount();

// Set a custom MAC address (persists in NVS)
bool setCustomMacAddress(const uint8_t* macAddress);

// Load and apply custom MAC address from NVS (called during setup)
void loadCustomMacAddress();

// Callbacks (must be accessible for ESP-NOW library) - ESP32 Arduino 2.x/3.x signatures
void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onEspNowDataReceived(const uint8_t *mac, const uint8_t *data, int len);

#endif // ESPNOW_HANDLER_H
