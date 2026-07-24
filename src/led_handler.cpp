#include "led_handler.h"
#include "config.h"
#include <FastLED.h>

// ---------------------------------------------------------------------------
// Built-in LED (single GPIO) – blink pattern state machine
// ---------------------------------------------------------------------------

#ifndef LED_PIN
#define LED_PIN 2
#endif
#ifndef LED_ACTIVE_LEVEL
#define LED_ACTIVE_LEVEL HIGH
#endif

#define LED_ON  (LED_ACTIVE_LEVEL)
#define LED_OFF (!(LED_ACTIVE_LEVEL))

static const uint8_t ledPin = LED_PIN;

// Pattern step arrays – even indices = ON durations, odd = OFF durations (ms)
static const uint16_t PATTERN_WIFI_STEPS[]   = { 80, 150, 80, 1690 }; // double blink / 2 s
static const uint16_t PATTERN_ESPNOW_STEPS[] = { 80, 1920 };           // single blink / 2 s
static const uint16_t PATTERN_RAPID_STEPS[]  = { 100, 100 };           // rapid 100ms toggle
static const uint16_t PATTERN_OTA_STEPS[]    = { 50, 50 };             // very rapid 50ms toggle

struct PatternDef { const uint16_t* steps; uint8_t numSteps; };
static const PatternDef PATTERNS[] = {
  { PATTERN_WIFI_STEPS,   4 },
  { PATTERN_ESPNOW_STEPS, 2 },
  { PATTERN_RAPID_STEPS,  2 },
  { PATTERN_OTA_STEPS,    2 }
};

// Sentinel 255 means "not yet set" – ensures first setLedPattern() always fires
static LedPattern    currentPattern      = (LedPattern)255;
static uint8_t       currentStep         = 0;
static unsigned long lastStepChangeMs    = 0;

static unsigned long builtinFlashExpMs   = 0;
static bool          builtinFlashing     = false;

// ---------------------------------------------------------------------------
// WS2812 RGB strip (FastLED)
// ---------------------------------------------------------------------------

#ifndef WS2812_DATA_PIN
#define WS2812_DATA_PIN 4
#endif
#ifndef WS2812_NUM_LEDS
#define WS2812_NUM_LEDS 8
#endif

#define STATUS_BRIGHTNESS   40   // 0-255 – comfortable indoor brightness

// LED 0 periodic mode-flash timings
#define MODE_FLASH_ON_MS   200   // lit for 200 ms …
#define MODE_FLASH_OFF_MS 3000   // … then dark for 3 s

static CRGB stripLeds[WS2812_NUM_LEDS];

// Per-LED one-shot timed flash state
struct StripFlash {
  bool         active;
  unsigned long expirationMs;
  CRGB         color;
};
static StripFlash stripFlashes[WS2812_NUM_LEDS];

// LED 0 mode-flash state
static bool          modeFlashIsOn         = false;
static unsigned long modeFlashLastChangeMs = 0;

// Map operating mode → colour for LED 0
static CRGB patternToColor(LedPattern p) {
  switch (p) {
    case LED_ESPNOW_MODE: return CRGB(0,   200, 0);   // green
    case LED_WIFI_MODE:   return CRGB(0,   0,   200); // blue
    case LED_OTA_BLINK:   return CRGB(200, 180, 0);   // yellow
    case LED_RAPID_BLINK: return CRGB(200, 0,   0);   // red
    default:              return CRGB(100, 0,   0);
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void setupLed() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LED_OFF);
  lastStepChangeMs = millis();

  FastLED.addLeds<WS2812B, WS2812_DATA_PIN, GRB>(stripLeds, WS2812_NUM_LEDS);
  FastLED.setBrightness(STATUS_BRIGHTNESS);

  // Startup self-test: sweep Red → Green → Blue so we know the strip responds
  const CRGB testColors[] = { CRGB(200, 0, 0), CRGB(0, 200, 0), CRGB(0, 0, 200) };
  for (int c = 0; c < 3; c++) {
    fill_solid(stripLeds, WS2812_NUM_LEDS, testColors[c]);
    FastLED.show();
    delay(300);
  }
  fill_solid(stripLeds, WS2812_NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void setLedPattern(LedPattern pattern) {
  if (currentPattern == pattern && !builtinFlashing) return;

  currentPattern    = pattern;
  currentStep       = 0;
  lastStepChangeMs  = millis();
  digitalWrite(ledPin, LED_ON);

  // Start mode-flash in ON phase so the colour is visible immediately
  modeFlashIsOn         = true;
  modeFlashLastChangeMs = millis();
  if (!stripFlashes[0].active) {
    stripLeds[0] = patternToColor(pattern);
    FastLED.show();
  }
}

void triggerLedFlash() {
  builtinFlashExpMs = millis() + 60;
  builtinFlashing   = true;
  digitalWrite(ledPin, LED_ON);
}

void triggerStripFlash(uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs) {
  if (ledIndex >= WS2812_NUM_LEDS) return;
  stripFlashes[ledIndex] = { true, millis() + durationMs, CRGB(r, g, b) };
  stripLeds[ledIndex]    = CRGB(r, g, b);
  FastLED.show();
}

void updateLed() {
  unsigned long now     = millis();
  bool          needShow = false;

  // ── Built-in LED ─────────────────────────────────────────────────────────
  if (builtinFlashing) {
    if (now >= builtinFlashExpMs) {
      builtinFlashing  = false;
      lastStepChangeMs = now;
      digitalWrite(ledPin, (currentStep % 2 == 0) ? LED_ON : LED_OFF);
    } else {
      digitalWrite(ledPin, LED_ON);
    }
  } else if (currentPattern != (LedPattern)255) {
    const PatternDef& pat = PATTERNS[currentPattern];
    if (now - lastStepChangeMs >= pat.steps[currentStep]) {
      currentStep      = (currentStep + 1) % pat.numSteps;
      lastStepChangeMs = now;
      digitalWrite(ledPin, (currentStep % 2 == 0) ? LED_ON : LED_OFF);
    }
  }

  // ── WS2812 LED 0: periodic mode flash ────────────────────────────────────
  {
    uint16_t phaseDuration = modeFlashIsOn ? MODE_FLASH_ON_MS : MODE_FLASH_OFF_MS;
    if (now - modeFlashLastChangeMs >= phaseDuration) {
      modeFlashIsOn         = !modeFlashIsOn;
      modeFlashLastChangeMs = now;
      if (!stripFlashes[0].active) {
        stripLeds[0] = modeFlashIsOn ? patternToColor(currentPattern) : CRGB::Black;
        needShow = true;
      }
    }
  }

  // ── WS2812 all LEDs: handle one-shot timed flashes ────────────────────────
  for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
    if (!stripFlashes[i].active) continue;
    if (now >= stripFlashes[i].expirationMs) {
      stripFlashes[i].active = false;
      // Restore: LED 0 goes back to mode-flash colour, others go dark
      stripLeds[i] = (i == 0)
                       ? (modeFlashIsOn ? patternToColor(currentPattern) : CRGB::Black)
                       : CRGB::Black;
      needShow = true;
    }
    // Colour is already written to stripLeds[i] by triggerStripFlash()
  }

  if (needShow) {
    FastLED.show();
  }
}
