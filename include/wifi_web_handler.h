#ifndef WIFI_WEB_HANDLER_H
#define WIFI_WEB_HANDLER_H

#include <Arduino.h>

enum DeviceState {
  STATE_WIFI,
  STATE_ESPNOW
};

// Initialize Wi-Fi connection, Web server, and OTA handlers
void setupWifiWeb();

// Run periodic checks (countdown, OTA handle)
void handleWifiWeb();

// Shut down Wi-Fi connection and Web server
void stopWifiWeb();

// Cleanly disconnect Wi-Fi and initialize ESP-NOW
void transitionToEspNow();

// Stop ESP-NOW and re-enter Wi-Fi mode (no automatic timeout – stays until toggled again)
void transitionToWifi();

// Get the current state of the device
DeviceState getCurrentState();

// Check if dry run mode is enabled
bool isDryRunEnabled();

// Prolong the Wi-Fi mode time by the given milliseconds
void prolongWifiTime(uint32_t ms);

// Enable/disable dry run mode (persists to NVS)
void setDryRunMode(bool enable);

// Get the remaining seconds in Wi-Fi mode (-1 if infinite/dry-run)
int32_t getRemainingWifiTimeSec();

// Feed the software watchdog to prevent reboots during long blocking operations like OTA
void feedWatchdog();

#endif // WIFI_WEB_HANDLER_H
