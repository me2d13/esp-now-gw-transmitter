#include "espnow_handler.h"
#include "config.h"
#include "crypto.h"
#include <ESP8266WiFi.h>
#include <espnow.h>

#define MAX_PEERS 20

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
    Serial.println("[TRANS] ERROR: Peer list full (max 20 peers)");
    return false;
  }
  
  // Add new peer
  memcpy(peerList[peerCount], peerAddress, 6);
  peerCount++;
  
  int addPeerResult = esp_now_add_peer((uint8_t*)peerAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  if (addPeerResult != 0) {
    Serial.print("[TRANS] ERROR: Failed to add peer, code: ");
    Serial.println(addPeerResult);
    peerCount--; // Rollback
    return false;
  }
  
  Serial.println("[TRANS] New peer added");
  return true;
}

bool setupEspNow() {
  bool initSuccess = true;
  unsigned long initStartTime = millis();
  
  // Set device as a Wi-Fi Station
  Serial.println("[TRANS] Setting WiFi mode to STA...");
  WiFi.mode(WIFI_STA);
  delay(100);

  // Init ESP-NOW with retry logic
  Serial.println("[TRANS] Initializing ESP-NOW...");
  int espNowInitResult = esp_now_init();
  
  if (espNowInitResult != 0) {
    Serial.print("[TRANS] ERROR: ESP-NOW initialization failed with code: ");
    Serial.println(espNowInitResult);
    Serial.print("[TRANS] Will wait ");
    Serial.print(INIT_RETRY_TIMEOUT_MS / 1000);
    Serial.println(" seconds and then reboot...");
    initSuccess = false;
  }
  
  if (initSuccess) {
    // Set role and register callbacks
    if (esp_now_set_self_role(ESP_NOW_ROLE_COMBO) != 0) {
      Serial.println("[TRANS] ERROR: Failed to set ESP-NOW role");
      initSuccess = false;
    }
  }
  
  if (initSuccess) {
    if (esp_now_register_send_cb(onEspNowDataSent) != 0) {
      Serial.println("[TRANS] ERROR: Failed to register send callback");
      initSuccess = false;
    }
  }
  
  if (initSuccess) {
    if (esp_now_register_recv_cb(onEspNowDataReceived) != 0) {
      Serial.println("[TRANS] ERROR: Failed to register receive callback");
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
    
    Serial.println("[TRANS] Rebooting now...");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
  
  Serial.println("[TRANS] ESP-NOW transmitter started successfully!");
  Serial.print("[TRANS] MAC Address: ");
  Serial.println(WiFi.macAddress());
  
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
    Serial.println("[TRANS] ERROR: Empty message");
    return;
  }
  
  // Prepare data for sending
  uint8_t dataBytes[250];
  int length = messageToByteArray(dataStr.c_str(), dataBytes, ENABLE_ENCRYPTION);
  
  if (length > 0) {
    Serial.print("[TRANS] Sending ");
    logMessageToSerial(dataBytes, length, ENABLE_ENCRYPTION);
    
    int sendResult = esp_now_send(peerAddress, dataBytes, length);
    if (sendResult != 0) {
      Serial.print("[TRANS] ERROR: esp_now_send failed with code: ");
      Serial.println(sendResult);
    }
  }
}

uint8_t getEspNowPeerCount() {
  return peerCount;
}

// Callback when data is sent
void onEspNowDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("[TRANS] Last espnow send status: ");
  if (sendStatus == 0) {
    Serial.println("Delivery success");
  } else {
    Serial.println("Delivery fail");
  }
}

// Callback when data is received
void onEspNowDataReceived(unsigned char* mac, unsigned char *incomingData, unsigned char len) {
  memcpy(espNowMessageBuffer, incomingData, len);
  Serial.print("[TRANS] From esp-now received ");
  Serial.print(len);
  Serial.print(" bytes: ");
  logMessageToSerial(incomingData, len, ENABLE_ENCRYPTION);
  Serial.print("DATA:{\"mac\":\"");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
  }
  Serial.print("\",\"message\":");
  if (ENABLE_ENCRYPTION) {
    inPlaceDecrypt(espNowMessageBuffer, len);
  }
  Serial.print((char *) espNowMessageBuffer);
  Serial.println("}");
}
