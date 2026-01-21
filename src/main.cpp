#include <Arduino.h>
#include "config.h"
#include "crypto.h"
#include "serial_handler.h"
#include "espnow_handler.h"

// LED control variables
byte ledPin = LED_BUILTIN;
bool ledState = HIGH;
unsigned long ledToggleTime = 0;
int ledBlinkCount = 0;
int ledBlinkTarget = 0;

// Software watchdog
unsigned long lastLoopTime = 0;

// Timing variables
unsigned long lastBlinkMillis = 0;
unsigned long lastHeartbeatMillis = 0;

// Non-blocking LED blink - call this from loop()
void updateLedBlink() {
  if (ledBlinkTarget > 0) {
    unsigned long currentMillis = millis();
    if (currentMillis - ledToggleTime >= 100) {
      ledToggleTime = currentMillis;
      ledState = !ledState;
      digitalWrite(ledPin, ledState);
      
      if (!ledState) { // Count on LOW transitions
        ledBlinkCount++;
        if (ledBlinkCount >= ledBlinkTarget) {
          ledBlinkTarget = 0;
          ledBlinkCount = 0;
          ledState = HIGH;
          digitalWrite(ledPin, HIGH);
        }
      }
    }
  }
}

// Start a non-blocking blink sequence
void startBlink(int times) {
  ledBlinkTarget = times;
  ledBlinkCount = 0;
  ledToggleTime = millis();
}

void setup() {
  // Initialize LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  
  // Initialize crypto
  setupCrypto();
  
  // Initialize serial communication
  setupSerial();
  
  // Initialize ESP-NOW (with auto-reboot on failure)
  setupEspNow();
  
  // Initialize watchdog
  lastLoopTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Software watchdog - check if loop is running
  if (currentMillis - lastLoopTime > WATCHDOG_TIMEOUT_S * 1000UL) {
    Serial.println("[TRANS] ERROR: Watchdog timeout - system appears hung");
    Serial.println("[TRANS] Rebooting...");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
  lastLoopTime = currentMillis;
  
  // Update non-blocking LED blink
  updateLedBlink();
  
  // Blink LED 1 time every 10 seconds
  if (currentMillis - lastBlinkMillis >= 10000) {
    lastBlinkMillis = currentMillis;
    startBlink(1);
  }
  
  // Send heartbeat message
  if (currentMillis - lastHeartbeatMillis >= HEART_BEAT_S * 1000UL) {
    lastHeartbeatMillis = currentMillis;
    Serial.print("[TRANS] Heartbeat from ");
    Serial.print(WHO_AM_I);
    Serial.print(" - Uptime: ");
    Serial.print(currentMillis / 1000);
    Serial.print("s, Peers: ");
    Serial.println(getEspNowPeerCount());
  }
  
  // Handle incoming serial messages
  if (readSerialMessage()) {
    handleSerialMessage();
  }
  
  // Small yield to prevent watchdog issues
  yield();
}
