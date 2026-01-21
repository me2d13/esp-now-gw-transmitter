#include "espnow_handler.h"
#include "config.h"
#include "crypto.h"
#include "logger.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

// Define LED_BUILTIN for ESP32 (not defined by default)
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

#define MAX_PEERS 20
#define NVS_NAMESPACE "espnow_gw"
#define NVS_MAC_KEY "custom_mac"

// NVS storage
static Preferences preferences;

// Peer management
static uint8_t peerList[MAX_PEERS][6];
static uint8_t peerCount = 0;

// Message buffer for ESP-NOW
static byte espNowMessageBuffer[251];

// Helper function to convert hex string to byte array
static void charToByteArray(const char* charArray, uint8_t* byteArray) {
  for (int i = 0; i < 6; i++) {
    char hex[3];
    hex[0] = charArray[i * 2];
    hex[1] = charArray[i * 2 + 1];
    hex[2] = '\0';
    byteArray[i] = strtol(hex, NULL, 16);
  }
}

// Helper function to add a peer if not already in the list
static bool addPeerIfNeeded(const uint8_t* peerAddress) {
  // Check if peer already exists
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peerList[i], peerAddress, 6) == 0) {
      return true; // Already exists
    }
  }
  
  // Check if peer list is full
  if (peerCount >= MAX_PEERS) {
    logPrintln("[TRANS] ERROR: Peer list full (max 20 peers)");
    return false;
  }
  
  // Add new peer (ESP32 API)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;  // Use current channel
  peerInfo.encrypt = false;
  
  esp_err_t addPeerResult = esp_now_add_peer(&peerInfo);
  if (addPeerResult != ESP_OK) {
    logPrint("[TRANS] ERROR: Failed to add peer, code: ");
    logPrintln(addPeerResult);
    return false;
  }
  
  // Track in our list
  memcpy(peerList[peerCount], peerAddress, 6);
  peerCount++;
  
  logPrintln("[TRANS] New peer added");
  return true;
}

bool setupEspNow() {
  bool initSuccess = true;
  unsigned long initStartTime = millis();
  
  // Ensure WiFi is in STA mode (may have been set by loadCustomMacAddress)
  if (WiFi.getMode() != WIFI_STA) {
    logPrintln("[TRANS] Setting WiFi mode to STA...");
    WiFi.mode(WIFI_STA);
    delay(100);
  }

  // Init ESP-NOW with retry logic (ESP32 API)
  logPrintln("[TRANS] Initializing ESP-NOW...");
  esp_err_t espNowInitResult = esp_now_init();
  
  if (espNowInitResult != ESP_OK) {
    logPrint("[TRANS] ERROR: ESP-NOW initialization failed with code: ");
    logPrintln(espNowInitResult);
    logPrint("[TRANS] Will wait ");
    logPrint(INIT_RETRY_TIMEOUT_MS / 1000);
    logPrintln(" seconds and then reboot...");
    initSuccess = false;
  }
  
  if (initSuccess) {
    // Register callbacks (ESP32 API)
    if (esp_now_register_send_cb(onEspNowDataSent) != ESP_OK) {
      logPrintln("[TRANS] ERROR: Failed to register send callback");
      initSuccess = false;
    }
  }
  
  if (initSuccess) {
    if (esp_now_register_recv_cb(onEspNowDataReceived) != ESP_OK) {
      logPrintln("[TRANS] ERROR: Failed to register receive callback");
      initSuccess = false;
    }
  }
  
  if (!initSuccess) {
    // Blink LED rapidly to indicate error
    pinMode(LED_BUILTIN, OUTPUT);
    while (millis() - initStartTime < INIT_RETRY_TIMEOUT_MS) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
    
    logPrintln("[TRANS] Rebooting now...");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
  
  logPrintln("[TRANS] ESP-NOW transmitter started successfully!");
  logPrint("[TRANS] MAC Address: ");
  logPrintln(WiFi.macAddress());
  
  return true;
}

void sendEspNowMessage(const char* macAddress, JsonObject messageObj) {
  uint8_t peerAddress[6];
  charToByteArray(macAddress, peerAddress);
  
  // Add peer if needed
  if (!addPeerIfNeeded(peerAddress)) {
    return;
  }
  
  // Convert message object to string
  String dataStr;
  serializeJson(messageObj, dataStr);
  
  if (dataStr.length() == 0) {
    logPrintln("[TRANS] ERROR: Empty message");
    return;
  }
  
  // Prepare data for sending
  uint8_t dataBytes[250];
  int length = messageToByteArray(dataStr.c_str(), dataBytes, ENABLE_ENCRYPTION);
  
  if (length > 0) {
    logPrint("[TRANS] Sending ");
    logMessageToSerial(dataBytes, length, ENABLE_ENCRYPTION);
    
    esp_err_t sendResult = esp_now_send(peerAddress, dataBytes, length);
    if (sendResult != ESP_OK) {
      logPrint("[TRANS] ERROR: esp_now_send failed with code: ");
      logPrintln(sendResult);
    }
  }
}

uint8_t getEspNowPeerCount() {
  return peerCount;
}

// Callback when data is sent (ESP32 signature)
void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  logPrint("[TRANS] Last espnow send status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    logPrintln("Delivery success");
  } else {
    logPrintln("Delivery fail");
  }
}

// Callback when data is received (ESP32 Arduino 2.x/3.x signature)
void onEspNowDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
  memcpy(espNowMessageBuffer, data, len);
  logPrint("[TRANS] From esp-now received ");
  logPrint(len);
  logPrint(" bytes: ");
  logMessageToSerial((byte*)data, len, ENABLE_ENCRYPTION);
  logPrint("DATA:{\"mac\":\"");
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16) logPrint("0");
    logPrint(mac[i], HEX);
  }
  logPrint("\",\"message\":");
  if (ENABLE_ENCRYPTION) {
    inPlaceDecrypt(espNowMessageBuffer, len);
  }
  logPrint((char *) espNowMessageBuffer);
  logPrintln("}");
}

// Set a custom MAC address and save to NVS
bool setCustomMacAddress(const uint8_t* macAddress) {
  // Open NVS in write mode
  if (!preferences.begin(NVS_NAMESPACE, false)) {
    logPrintln("[TRANS] ERROR: Failed to open NVS for writing");
    return false;
  }
  
  // Store MAC address as bytes
  size_t written = preferences.putBytes(NVS_MAC_KEY, macAddress, 6);
  preferences.end();
  
  if (written != 6) {
    logPrintln("[TRANS] ERROR: Failed to write MAC to NVS");
    return false;
  }
  
  logPrintln("[TRANS] Custom MAC address saved to NVS");
  return true;
}

// Load custom MAC address from NVS and apply it
void loadCustomMacAddress() {
  // Open NVS in read-only mode
  if (!preferences.begin(NVS_NAMESPACE, true)) {
    // NVS not initialized yet, skip silently
    return;
  }
  
  // Check if MAC address exists
  if (!preferences.isKey(NVS_MAC_KEY)) {
    logPrintln("[TRANS] No custom MAC address configured, using default");
    preferences.end();
    return;
  }
  
  // Read MAC address
  uint8_t customMac[6];
  size_t len = preferences.getBytes(NVS_MAC_KEY, customMac, 6);
  preferences.end();
  
  if (len != 6) {
    logPrintln("[TRANS] ERROR: Invalid MAC address in NVS");
    return;
  }
  
  // Initialize WiFi in STA mode (required before setting MAC)
  WiFi.mode(WIFI_STA);
  
  // Apply the custom MAC address
  esp_err_t result = esp_wifi_set_mac(WIFI_IF_STA, customMac);
  if (result == ESP_OK) {
    logPrint("[TRANS] Custom MAC address loaded: ");
    for (int i = 0; i < 6; i++) {
      if (customMac[i] < 16) logPrint("0");
      logPrint(customMac[i], HEX);
    }
    logPrintln();
  } else {
    logPrint("[TRANS] ERROR: Failed to set custom MAC, code: ");
    logPrintln(result);
  }
}
