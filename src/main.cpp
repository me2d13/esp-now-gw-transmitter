#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "crypto.h"
#include "logger.h"
#include "serial_handler.h"
#include "espnow_handler.h"
#include "wifi_web_handler.h"
#include "led_handler.h"

// Software watchdog
unsigned long lastLoopTime = 0;

// Timing variables
unsigned long lastHeartbeatMillis = 0;

// Feed the software watchdog from other files
void feedWatchdog() {
  lastLoopTime = millis();
}

void setup() {
  // IMPORTANT: Load custom MAC address FIRST, before any WiFi initialization
  // This must be done before WiFi.mode() is called anywhere
  loadCustomMacAddress();
  
  // Initialize LED handler
  setupLed();
  
  // Initialize crypto
  setupCrypto();
  
  // Initialize serial communication
  setupSerial();
  
  // Initialize Wi-Fi & Web Server Mode (switches to ESP-NOW after timeout/command)
  setupWifiWeb();
  
  // Initialize watchdog
  lastLoopTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  
  
  // Software watchdog - check if loop is running
  if (currentMillis - lastLoopTime > WATCHDOG_TIMEOUT_S * 1000UL) {
    logPrintln("[TRANS] ERROR: Watchdog timeout - system appears hung");
    logPrintln("[TRANS] Rebooting...");
    Serial.flush();
    getUART2().flush();
    delay(100);
    ESP.restart();
  }
  lastLoopTime = currentMillis;

  // Handle Wi-Fi mode state machine (OTA and timeout checks)
  handleWifiWeb();
  
  // Update non-blocking LED pattern generator
  updateLed();
  
  // Send heartbeat message
  if (currentMillis - lastHeartbeatMillis >= HEART_BEAT_S * 1000UL) {
    lastHeartbeatMillis = currentMillis;

    // Flash LED 1 orange (1 s) instead of logging – avoids flooding in-memory log
    triggerStripFlash(1, 220, 100, 0, 1000);

    // Gateway JSON heartbeat
    JsonDocument hb;
    hb["type"] = "heartbeat";
    hb["device"] = WHO_AM_I;
    hb["mac"] = WiFi.macAddress();
    hb["uptime"] = currentMillis / 1000;
    hb["peers"] = getEspNowPeerCount();
    hb["free_heap"] = ESP.getFreeHeap();
    sendGatewayMessage(hb);
  }
  
  // Handle incoming serial messages
  if (readSerialMessage()) {
    handleSerialMessage();
  }
  
  // Small yield to prevent watchdog issues
  yield();
}
