# ESP-NOW Gateway Transmitter

This is the ESP-NOW communication component of the MQTT ↔ ESP-NOW gateway system. It runs on an ESP32 and handles bidirectional message translation between serial communication and ESP-NOW protocol.

## Overview

This component is part of a larger home automation project that enables control of ESP-NOW devices (like relays) via MQTT. The complete system is described at [esp-now-gw-mqtt](https://github.com/me2d13/esp-now-gw-mqtt).

### System Architecture

```
MQTT Broker ←→ [esp-now-gw-mqtt] ←(serial)→ [esp-now-gw-transmitter] ←(ESP-NOW)→ ESP-NOW Devices
                (WiFi/MQTT part)                  (ESP-NOW part)                    (relays, remotes, etc.)
```

## Features

### Core Functionality
- **Bidirectional Communication**: Translates between serial JSON messages and ESP-NOW protocol.
- **Message Encryption**: Optional AES-CTR encryption for secure ESP-NOW communication.
- **Dynamic Peer Management**: Automatically adds new ESP-NOW peers as needed (up to 20 peers).
- **Message Validation**: Comprehensive JSON validation before processing.

### Wi-Fi Startup & Maintenance Mode
- **Wi-Fi Boot Phase**: Connects to your local Wi-Fi network at boot (using credentials in `config.h`) for a setup period (default: 3 minutes) before starting ESP-NOW.
- **Web UI Dashboard**: Serves a self-contained, responsive, dark-mode status page at the device's IP. Shows uptime, free memory, MAC address, active peers, and a dynamic countdown timer.
- **Stay in Wi-Fi / Dry Run Mode**: A toggle in the Web UI pauses the countdown timer to stay in Wi-Fi mode indefinitely. While in Wi-Fi mode, ESP-NOW acts in "Dry Run" mode where serial commands are logged as simulations but not transmitted, allowing easy debugging.
- **Circular Memory Logger**: Captures and buffers the last 100 log lines with relative boot-time timestamps (`HH:MM:SS.mmm`), accessible directly in the Web UI.
- **Dual OTA Uploads**: 
  - **Web OTA**: Upload `firmware.bin` directly through any browser with a visual progress bar.
  - **ArduinoOTA**: Upload wirelessly from VSCode/PlatformIO during the Wi-Fi boot phase.

### Robustness Features
- **Auto-Recovery**: Automatically reboots if ESP-NOW initialization fails.
- **Software Watchdog**: Monitors loop execution and reboots if the system hangs. The watchdog is automatically fed during OTA flashes to prevent accidental reboots.
- **Error Handling**: Validates all ESP-NOW API calls with detailed error reporting.
- **Non-blocking Operation**: LED status blinking and all operations are non-blocking.
- **Buffer Management**: Proper buffer clearing to prevent data corruption.
- **Dual-Output Logging**: All messages sent to both UART2 (MQTT module) and USB (debugging).
- **Custom MAC Address**: Persistent MAC address configuration via NVS.

## Hardware Requirements

- **ESP32** (ESP32-WROOM-32 or compatible)
- Serial connection to the MQTT gateway component via **UART2**:
  - **TX**: GPIO17
  - **RX**: GPIO16
  - **Baud**: 115200
- **Optional**: USB connection (UART0) for debugging
  - Both USB and UART2 can be connected simultaneously.
  - All log messages are sent to both outputs.
- **Optional**: WS2812B LED strip (8 LEDs recommended) connected to **GPIO4** for visual status indicators.
- **Optional**: Momentary push-button / microswitch connected to **GPIO33** for manual mode toggling.

### Dual-Output Logging

The transmitter features a dual-output logging system:

- **UART2 (GPIO16/17)**: Primary communication with MQTT gateway module
  - Receives commands from MQTT module
  - Sends all log messages and data to MQTT module
  - Used for production operation
  
- **USB Serial (UART0)**: Debugging output
  - Receives a copy of all log messages
  - Used for development and troubleshooting
  - Can be connected simultaneously with UART2

This allows you to keep USB connected for debugging while the MQTT module operates normally on UART2.

## Message Protocol

### Serial → ESP-NOW (Incoming)

Messages received on serial must be JSON formatted with the following structure:

```json
{
  "to": "ECFABC2FE867",
  "message": {
    "channel": 1,
    "push": 500
  }
}
```

- `to`: MAC address of the target ESP-NOW device (12 hex characters, no colons)
- `message`: JSON object containing the actual message to send

### ESP-NOW → Serial (Outgoing)

Messages received via ESP-NOW are forwarded to serial with the `DATA:` prefix:

```
DATA:{"mac":"ECFABC2FE867","message":{"status":"ok"}}
```

Any serial output without the `DATA:` prefix is considered a log message.

### Command Messages

The transmitter supports special command messages for device management:

#### Ping Command
Check if the device is alive and get status information:

```json
{"command": "ping"}
```

Response (log message):
```
[TRANS] PING response from ESP32-ESPNOW-GW-TRANS - MAC: AA:BB:CC:DD:EE:FF, Uptime: 12345s, Peers: 3, Free Heap: 25600 bytes
```

#### Reset Command
Remotely reboot the device:

```json
{"command": "reset"}
```

The device will immediately reboot after acknowledging the command.

#### Get MAC Command
Display the current MAC address:

```json
{"command": "get-mac"}
```

Response (log message):
```
[TRANS] Current MAC address: AA:BB:CC:DD:EE:FF
```

#### Set MAC Command
Set a custom MAC address (persists across reboots):

```json
{"command": "set-mac", "value": "AABBCCDDEEFF"}
```

The MAC address must be exactly 12 hexadecimal characters (no colons or separators). The device will save the MAC address to NVS (Non-Volatile Storage) and automatically reboot to apply it. On subsequent boots, the custom MAC will be loaded and applied automatically.

**Note**: This is useful when other ESP-NOW devices have hardcoded the gateway's MAC address.

## Configuration

Edit `include/config.h` to customize:

### Wi-Fi Settings
```cpp
#define WIFI_SSID "your-ssid"          // Your Wi-Fi network SSID
#define WIFI_PASSWORD "your-password"  // Your Wi-Fi network password
#define WIFI_TIME_MS (3 * 60 * 1000)   // Default setup phase time (3 minutes)
```

### Encryption
```cpp
#define CRYPTO_KEY {0x11, 0xb5, ...}  // 32-byte AES key
#define ENABLE_ENCRYPTION 1            // 1 = enabled, 0 = disabled
```

### Heartbeat
```cpp
#define HEART_BEAT_S 60*60  // Heartbeat interval in seconds (default: 1 hour)
```

### Device Identity
```cpp
#define WHO_AM_I "ESP32-ESPNOW-GW-TRANS"  // Device identifier
#define SW_VERSION "0.2"                  // Software version
```

### Robustness Settings
```cpp
#define INIT_RETRY_TIMEOUT_MS 60000  // Wait time before reboot on init failure (default: 60s)
#define WATCHDOG_TIMEOUT_S 30        // Software watchdog timeout (default: 30s)
```

## Building and Uploading

This project uses PlatformIO:

```bash
# Build firmware
pio run

# Upload via USB/COM port
pio run --target upload

# Upload wirelessly via OTA (requires Wi-Fi mode)
pio run -t upload -e ota

# Monitor serial output
pio device monitor
```

Default upload port is `COM6` (configure in `platformio.ini`).

## LED Indicators

The firmware uses two indicator outputs: the **built-in GPIO LED** and an optional **WS2812B RGB strip**.

### Built-in LED (GPIO 2)

> [!NOTE]
> Some ESP32 development boards (such as the official Espressif ESP32-DevKitC V4) do not have a user-programmable LED onboard (only a hardwired red power LED). If your board does not have an onboard user LED, you can connect an external LED with a suitable resistor to GPIO 2 to see these patterns.

| Pattern | Meaning |
|---|---|
| Double blink every 2 s | Wi-Fi / Maintenance mode |
| Single blink every 2 s | Normal ESP-NOW mode |
| Very rapid blink (50 ms) | OTA firmware update in progress |
| Rapid blink (100 ms) | Initialization error – waiting to reboot |
| Momentary 60 ms flash | ESP-NOW message sent or received |

### WS2812B RGB Strip (GPIO 4)

Connect the data line of an 8-LED WS2812B module to **GPIO 4** (3.3 V signal level is accepted by most WS2812B modules). Power the strip from the **5 V** rail; share GND with the ESP32.

> [!NOTE]
> On every boot the strip runs a short self-test: all LEDs cycle **Red → Green → Blue** (≈ 300 ms each). If you see this sweep, wiring and library are working correctly.

#### LED 0 — Operating mode

Flashes briefly (200 ms on, 3 s off) to show the current mode **and** to confirm the main loop is still executing.

| Colour | Mode |
|---|---|
| 🔵 Blue | Wi-Fi / Maintenance mode |
| 🟢 Green | Normal ESP-NOW mode |
| 🟡 Yellow | OTA firmware update in progress |
| 🔴 Red | Initialization error / error state |

#### LED 1 — Heartbeat

Flashes **🟠 orange for 1 second** each time a heartbeat message is sent to the gateway. The heartbeat interval is configured by `HEART_BEAT_S` in `config.h` (default: 60 s).

#### LEDs 2–7 — Reserved

Currently off; available for future use. LED 2 briefly flashes **white** as a confirmation when the mode-toggle button is pressed.

#### Configuration

```cpp
// config.h
#define WS2812_DATA_PIN 4   // GPIO pin for the strip data line
#define WS2812_NUM_LEDS 8   // Number of LEDs on the strip
```

## Mode-Toggle Button

A momentary push-button (or microswitch) connected to **GPIO33** lets you switch between ESP-NOW and Wi-Fi mode without a reboot or serial command.

### Wiring

```
ESP32 GPIO33  ────────┤ switch ├──── GND
```

No external resistor is needed — GPIO33 uses the ESP32's internal pull-up. The firmware reads the pin as **active-LOW** (pressed = GND).

> [!CAUTION]
> Do **not** use GPIO0 for this switch. GPIO0 is connected to ADC2, which WiFi locks exclusively. Reading GPIO0 while WiFi is active causes `ESP_ERR_TIMEOUT` errors that crash the device. GPIO0 is also a strapping pin — holding it LOW at boot enters download mode.

### Press Actions

| Press duration | Action |
|---|---|
| Short press (< 5 s) | **Toggle mode**: ESP-NOW → Wi-Fi or Wi-Fi → ESP-NOW |
| Long press (≥ 5 s) | **Reboot** the ESP32 |

### Mode Toggle Behaviour

- When toggling **into Wi-Fi** mode, the Wi-Fi timeout timer is **disabled** (dry-run mode). The device stays in Wi-Fi indefinitely until the button is pressed again. This is intentional — you triggered it manually, so it should not auto-switch back.
- When toggling **into ESP-NOW** mode, Wi-Fi and the web server are cleanly shut down before ESP-NOW is initialised.
- In both directions the **custom MAC address** is re-applied from NVS before the new mode starts, so peers that have the MAC hardcoded continue to work.

### LED Feedback (WS2812B strip)

| LED | Event | Colour |
|---|---|---|
| LED 0 | Mode indicator (flashes every 3 s) | 🔵 Blue (Wi-Fi) / 🟢 Green (ESP-NOW) |
| LED 2 | Button press acknowledged | ⚪ White flash, 500 ms |
| LED 2+3 | Long press / reboot imminent | 🔴 Red flash, 200 ms |

### Configuration

```cpp
// config.h
#define BUTTON_PIN 33             // GPIO for the switch (connect other side to GND)
#define BUTTON_LONG_PRESS_MS 5000 // Hold duration in ms to trigger a reboot
```

The transmitter includes comprehensive error handling:

### Initialization Errors
If ESP-NOW fails to initialize:
1. Error is logged to serial with error code
2. LED blinks rapidly
3. After `INIT_RETRY_TIMEOUT_MS` (default 60s), device automatically reboots
4. Process repeats until successful initialization

### Runtime Errors
- **Watchdog timeout**: If loop() doesn't execute within `WATCHDOG_TIMEOUT_S`, device reboots
- **Invalid JSON**: Logged and ignored, device continues operation
- **Peer list full**: Error logged when attempting to add more than 20 peers
- **Send failures**: ESP-NOW send errors are logged with error codes

## Serial Communication

### UART2 (MQTT Module Connection)
- **Baud rate**: 115200
- **TX Pin**: GPIO17
- **RX Pin**: GPIO16
- **Format**: JSON messages terminated with newline (`\n`)
- **Purpose**: Primary communication with MQTT gateway module

### USB Serial (Debugging)
- **Baud rate**: 115200
- **Pins**: GPIO1 (TX), GPIO3 (RX) - default UART0
- **Purpose**: Debugging and development
- **Note**: Receives a copy of all messages sent to UART2

### Wiring to MQTT Module

```
ESP32 Transmitter        MQTT Gateway Module
-----------------        -------------------
GPIO17 (TX)      ------>  RX
GPIO16 (RX)      <------  TX
GND              ------>  GND
```

**Important**: Cross the TX/RX lines - TX on ESP32 connects to RX on MQTT module and vice versa.

## Dependencies

- **Arduino framework** for ESP32
- **ArduinoJson** (v7.3.0+) - JSON parsing and serialization
- **tiny-AES-c** - AES encryption for ESP-NOW messages
- **ESPAsyncWebServer** (v3.6.0+) - Asynchronous web server for UI & Web OTA
- **FastLED** (v3.9.0+) - WS2812B RGB LED strip control

## Changelog

### v0.2
- Added Wi-Fi Boot / Maintenance mode at startup.
- Implemented responsive, self-contained HTML/CSS/JS status Dashboard (`PROGMEM`-embedded).
- Added browser-based Web OTA upload with real-time progress bar.
- Added standard `ArduinoOTA` support for PlatformIO uploads.
- Added a "Stay in Wi-Fi Mode" UI toggle to pause the transition timer and run in "Dry Run" mode indefinitely for debugging.
- Added circular local memory logger (last 100 entries) with relative boot timestamps (`HH:MM:SS.mmm`).
- Fed software watchdog during OTA flashing to prevent reboots.
- Fixed JSON deprecation warnings.

### v0.1
- Initial release of ESP-NOW bidirectional gateway transmitter with encryption and dynamic peer management.

## License

Part of the [esp-now-gw-mqtt](https://github.com/me2d13/esp-now-gw-mqtt) project.

## Related Components

- **[esp-now-gw-mqtt](https://github.com/me2d13/esp-now-gw-mqtt)** - MQTT bridge (WiFi/MQTT part)
- **esp-now-relay** - Relay control module
- **esp-now-relay-remote** - Battery-powered remote control
