#include "button_handler.h"
#include "config.h"
#include "logger.h"
#include "led_handler.h"
#include "wifi_web_handler.h"

// Fallback defaults if not defined in config.h
#ifndef BUTTON_PIN
#define BUTTON_PIN 0
#endif
#ifndef BUTTON_LONG_PRESS_MS
#define BUTTON_LONG_PRESS_MS 5000
#endif

// Debounce window – ignore state changes shorter than this
#define DEBOUNCE_MS 50

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

enum ButtonState {
  BTN_IDLE,           // Not pressed
  BTN_DEBOUNCING,     // Just detected a press, waiting for debounce
  BTN_HELD,           // Confirmed press, tracking hold duration
  BTN_LONG_FIRED      // Long-press action already fired – wait for release
};

static ButtonState btnState           = BTN_IDLE;
static unsigned long btnEventStartMs  = 0;  // Timestamp of first press or debounce start
static bool lastRawLevel              = HIGH;

// ---------------------------------------------------------------------------

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // GPIO 0 already has a hardware pull-up; this is a safe redundant config
  lastRawLevel = digitalRead(BUTTON_PIN);
  logPrintf("[BTN] Button handler ready on GPIO%d (long-press threshold: %dms)\n",
            BUTTON_PIN, BUTTON_LONG_PRESS_MS);
}

static void onShortPress() {
  DeviceState state = getCurrentState();
  if (state == STATE_ESPNOW) {
    logPrintln("[BTN] Short press – switching to Wi-Fi mode");
    // Brief white flash on LED 2 to acknowledge
    triggerStripFlash(2, 200, 200, 200, 500);
    transitionToWifi();
  } else {
    logPrintln("[BTN] Short press – switching to ESP-NOW mode");
    triggerStripFlash(2, 200, 200, 200, 500);
    transitionToEspNow();
  }
}

static void onLongPress() {
  logPrintln("[BTN] Long press – rebooting...");
  // Flash red on all strip LEDs to signal reboot
  triggerStripFlash(2, 220, 0, 0, 200);
  triggerStripFlash(3, 220, 0, 0, 200);
  delay(300);
  ESP.restart();
}

void handleButton() {
  bool rawLevel      = digitalRead(BUTTON_PIN);
  unsigned long now  = millis();

  switch (btnState) {

    case BTN_IDLE:
      if (rawLevel == LOW && lastRawLevel == HIGH) {
        // Falling edge detected – start debounce
        btnState        = BTN_DEBOUNCING;
        btnEventStartMs = now;
      }
      break;

    case BTN_DEBOUNCING:
      if (rawLevel == HIGH) {
        // Bounced back up – ignore it
        btnState = BTN_IDLE;
      } else if (now - btnEventStartMs >= DEBOUNCE_MS) {
        // Stable LOW – confirmed press
        btnState        = BTN_HELD;
        btnEventStartMs = now;  // reset to track hold duration from confirmed press
      }
      break;

    case BTN_HELD:
      if (rawLevel == HIGH) {
        // Released before long-press threshold → short press
        onShortPress();
        btnState = BTN_IDLE;
      } else if (now - btnEventStartMs >= BUTTON_LONG_PRESS_MS) {
        // Still held past threshold → long press
        onLongPress();  // never returns (restarts)
        btnState = BTN_LONG_FIRED;
      }
      break;

    case BTN_LONG_FIRED:
      // Wait for release before going back to idle
      if (rawLevel == HIGH) {
        btnState = BTN_IDLE;
      }
      break;
  }

  lastRawLevel = rawLevel;
}
