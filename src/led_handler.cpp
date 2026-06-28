#include "led_handler.h"
#include "config.h"

// Fallbacks if not in config.h
#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef LED_ACTIVE_LEVEL
#define LED_ACTIVE_LEVEL HIGH
#endif

#define LED_ON  (LED_ACTIVE_LEVEL)
#define LED_OFF (!(LED_ACTIVE_LEVEL))

static const uint8_t ledPin = LED_PIN;

// Pattern step arrays (durations in milliseconds)
// Odd indices are OFF durations, even indices are ON durations
static const uint16_t PATTERN_WIFI_STEPS[] = { 80, 150, 80, 1690 }; // Double blink every 2 seconds
static const uint16_t PATTERN_ESPNOW_STEPS[] = { 80, 1920 };        // Single blink every 2 seconds
static const uint16_t PATTERN_RAPID_STEPS[] = { 100, 100 };         // Rapid toggle (100ms on, 100ms off)
static const uint16_t PATTERN_OTA_STEPS[] = { 50, 50 };             // Extremely rapid toggle (50ms on, 50ms off)

struct PatternDef {
  const uint16_t* steps;
  uint8_t numSteps;
};

static const PatternDef PATTERNS[] = {
  { PATTERN_WIFI_STEPS, 4 },
  { PATTERN_ESPNOW_STEPS, 2 },
  { PATTERN_RAPID_STEPS, 2 },
  { PATTERN_OTA_STEPS, 2 }
};

// State variables
static LedPattern currentPattern = LED_WIFI_MODE;
static uint8_t currentStep = 0;
static unsigned long lastStepChangeMs = 0;

static unsigned long flashExpirationMs = 0;
static bool isFlashing = false;

void setupLed() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LED_OFF);
  lastStepChangeMs = millis();
}

void setLedPattern(LedPattern pattern) {
  if (currentPattern == pattern && !isFlashing) return;
  
  currentPattern = pattern;
  currentStep = 0;
  lastStepChangeMs = millis();
  
  // Apply initial step state (ON)
  digitalWrite(ledPin, LED_ON);
}

void triggerLedFlash() {
  flashExpirationMs = millis() + 60; // Flash for 60ms
  isFlashing = true;
  digitalWrite(ledPin, LED_ON);
}

void updateLed() {
  unsigned long now = millis();
  
  // Handle momentary activity flash override
  if (isFlashing) {
    if (now >= flashExpirationMs) {
      isFlashing = false;
      // Reset pattern timer to continue smoothly
      lastStepChangeMs = now;
      digitalWrite(ledPin, (currentStep % 2 == 0) ? LED_ON : LED_OFF);
    } else {
      digitalWrite(ledPin, LED_ON);
      return;
    }
  }
  
  // Process the active blink pattern
  const PatternDef& pat = PATTERNS[currentPattern];
  uint16_t stepDuration = pat.steps[currentStep];
  
  if (now - lastStepChangeMs >= stepDuration) {
    currentStep = (currentStep + 1) % pat.numSteps;
    lastStepChangeMs = now;
    digitalWrite(ledPin, (currentStep % 2 == 0) ? LED_ON : LED_OFF);
  }
}
