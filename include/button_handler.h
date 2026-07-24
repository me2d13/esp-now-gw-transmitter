#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

// Initialize the mode-toggle button (configures GPIO with internal pull-up)
void setupButton();

// Poll button state and fire actions – call every loop() iteration (non-blocking)
//
// Actions:
//   Short press (release before BUTTON_LONG_PRESS_MS): toggle ESP-NOW ↔ Wi-Fi mode.
//       When toggling into Wi-Fi the timer is disabled (dry-run) so the device stays
//       in that mode until the button is pressed again.
//   Long press (held ≥ BUTTON_LONG_PRESS_MS): restart the ESP32.
void handleButton();

#endif // BUTTON_HANDLER_H
