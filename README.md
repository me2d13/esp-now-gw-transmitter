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

The built-in LED provides visual feedback on device status and operation:

> [!NOTE]
> Some ESP32 development boards (such as the official Espressif ESP32-DevKitC V4) do not have a user-programmable LED onboard (only a hardwired red power LED). If your board does not have an onboard user LED, you can connect an external LED with a suitable resistor to the configured GPIO pin (GPIO 2 by default) to see these status patterns.

- **Double blink every 2 seconds**: Device is in Wi-Fi Maintenance / Setup mode.
- **Single blink every 2 seconds**: Device is in normal ESP-NOW mode.
- **Very rapid blinking (50ms toggle)**: OTA firmware update in progress.
- **Rapid blinking (100ms toggle)**: Initialization error, waiting to reboot.
- **Momentary flash (60ms)**: Activity flash indicating an ESP-NOW message has been successfully sent or received.

## Error Handling

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
