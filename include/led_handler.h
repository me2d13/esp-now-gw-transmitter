#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <Arduino.h>

enum LedPattern {
  LED_WIFI_MODE,       // Double blink every 2 seconds (Wi-Fi maintenance mode)
  LED_ESPNOW_MODE,     // Single blink every 2 seconds (Normal ESP-NOW mode)
  LED_RAPID_BLINK,     // Rapid blinking (100ms on, 100ms off) - error state
  LED_OTA_BLINK        // Extremely rapid blinking (50ms on, 50ms off) - OTA update in progress
};

// Initialize the built-in LED pin
void setupLed();

// Set the current LED blink pattern
void setLedPattern(LedPattern pattern);

// Update LED state based on current pattern (non-blocking, call in loop)
void updateLed();

// Briefly flash the LED (e.g. on message transmit/receive)
void triggerLedFlash();

#endif // LED_HANDLER_H
