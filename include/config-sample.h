// rename this file to config.h and update values below

#ifndef CONFIG_H
#define CONFIG_H

// change to some random bytes which forms a key, same value must be used in clients and gateway
#define CRYPTO_KEY {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#define ENABLE_ENCRYPTION 1

// interval in seconds to send heartbeat message
#define HEART_BEAT_S 60*60

#define WHO_AM_I "ESP32-ESPNOW-GW-TRANS"

// Time to wait before rebooting if initialization fails (in milliseconds)
// Default: 60 seconds (60000 ms)
#define INIT_RETRY_TIMEOUT_MS 60000

// Software watchdog timeout in seconds
// The device will reboot if loop() doesn't execute within this time
#define WATCHDOG_TIMEOUT_S 30

#endif // CONFIG_H


