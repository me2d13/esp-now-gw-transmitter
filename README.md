# ESP-NOW Gateway Transmitter

This is the ESP-NOW communication component of the MQTT ↔ ESP-NOW gateway system. It runs on an ESP8266 and handles bidirectional message translation between serial communication and ESP-NOW protocol.

## Overview

This component is part of a larger home automation project that enables control of ESP-NOW devices (like relays) via MQTT. The complete system is described at [esp-now-gw-mqtt](https://github.com/me2d13/esp-now-gw-mqtt).

### System Architecture

```
MQTT Broker ←→ [esp-now-gw-mqtt] ←(serial)→ [esp-now-gw-transmitter] ←(ESP-NOW)→ ESP-NOW Devices
                (WiFi/MQTT part)                  (ESP-NOW part)                    (relays, remotes, etc.)
```

## Features

### Core Functionality
- **Bidirectional Communication**: Translates between serial JSON messages and ESP-NOW protocol
- **Message Encryption**: Optional AES-CTR encryption for secure ESP-NOW communication
- **Dynamic Peer Management**: Automatically adds new ESP-NOW peers as needed (up to 20 peers)
- **Message Validation**: Comprehensive JSON validation before processing

### Robustness Features
- **Auto-Recovery**: Automatically reboots if ESP-NOW initialization fails
- **Software Watchdog**: Monitors loop execution and reboots if system hangs
- **Error Handling**: Validates all ESP-NOW API calls with detailed error reporting
- **Non-blocking Operation**: LED blinking and all operations are non-blocking
- **Buffer Management**: Proper buffer clearing to prevent data corruption
- **Dual-Output Logging**: All messages sent to both UART2 (MQTT module) and USB (debugging)
- **Custom MAC Address**: Persistent MAC address configuration via NVS

## Hardware Requirements

- **ESP32** (ESP32-WROOM-32 or compatible)
- Serial connection to the MQTT gateway component via **UART2**:
  - **TX**: GPIO17
  - **RX**: GPIO16
  - **Baud**: 115200
- **Optional**: USB connection (UART0) for debugging
  - Both USB and UART2 can be connected simultaneously
  - All log messages are sent to both outputs

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
```

### Robustness Settings
```cpp
#define INIT_RETRY_TIMEOUT_MS 60000  // Wait time before reboot on init failure (default: 60s)
#define WATCHDOG_TIMEOUT_S 30        // Software watchdog timeout (default: 30s)
```

## Building and Uploading

This project uses PlatformIO:

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor
```

Default upload port is `COM6` (configure in `platformio.ini`).

## LED Indicators

The built-in LED provides status information:

- **Single blink every 10 seconds**: Normal operation
- **Rapid blinking**: Initialization error, waiting to reboot
- **Solid on**: Idle state

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

## Example Messages

### Relay Control
```json
{"to":"ECFABC2FE867","message":{"channel":1,"push":500}}
```

### State Change
```json
{"to":"ECFABC2FE867","message":{"index":1,"state":1}}
```

### Blink Command
```json
{"to":"ECFABC2FE867","message":{"blinkTimes":8}}
```

### Ping Device
```json
{"command":"ping"}
```

### Reset Device
```json
{"command":"reset"}
```

### Get MAC Address
```json
{"command":"get-mac"}
```

### Set Custom MAC Address
```json
{"command":"set-mac","value":"AABBCCDDEEFF"}
```


## Troubleshooting

### Device keeps rebooting
- Check serial output for initialization error codes
- Verify WiFi mode is set correctly (STA mode required for ESP-NOW)
- Ensure ESP-NOW is supported on your ESP32 board

### Messages not being sent
- Verify JSON format is correct (use `"to"` and `"message"` fields)
- Check that MAC address is exactly 12 hex characters
- Monitor serial output for error messages
- Verify encryption settings match on both ends

### No response from ESP-NOW devices
- Ensure encryption keys match on all devices
- Verify target device MAC address is correct
- Check that target device is in range and powered on

## License

Part of the [esp-now-gw-mqtt](https://github.com/me2d13/esp-now-gw-mqtt) project.

## Related Components

- **[esp-now-gw-mqtt](https://github.com/me2d13/esp-now-gw-mqtt)** - MQTT bridge (WiFi/MQTT part)
- **esp-now-relay** - Relay control module
- **esp-now-relay-remote** - Battery-powered remote control
