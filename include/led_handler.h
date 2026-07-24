#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <Arduino.h>

enum LedPattern {
  LED_WIFI_MODE,       // Wi-Fi maintenance mode
  LED_ESPNOW_MODE,     // Normal ESP-NOW mode
  LED_RAPID_BLINK,     // Rapid blinking - error state
  LED_OTA_BLINK        // OTA update in progress
};

// Initialize built-in LED and WS2812 strip (runs startup self-test)
void setupLed();

// Set the current operating mode. Updates LED 0 periodic flash color.
// Built-in LED also switches blink pattern.
void setLedPattern(LedPattern pattern);

// Non-blocking LED state machine – call every loop() iteration
void updateLed();

// Briefly flash the built-in LED (e.g. on ESP-NOW message activity)
void triggerLedFlash();

// Trigger a one-shot timed flash on a WS2812 strip LED.
//   ledIndex  : 0-based index into the strip (0 = status, 1 = heartbeat, …)
//   r, g, b   : LED color
//   durationMs: how long to keep the LED lit
// While active the flash overrides any other state on that LED.
void triggerStripFlash(uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs);

#endif // LED_HANDLER_H
