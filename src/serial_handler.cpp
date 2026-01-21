#include "serial_handler.h"
#include "espnow_handler.h"
#include "config.h"
#include <ESP8266WiFi.h>

#define SERIAL_BUFFER_SIZE 500
#define SERIAL_BAUDRATE 115200

// Serial message buffer
static char serialMessageBuffer[SERIAL_BUFFER_SIZE];

// JSON document for parsed messages
static JsonDocument doc;

void setupSerial() {
  // Swap serial pins to use GPIO13 as RX and GPIO15 as TX
  Serial.swap();
  Serial.begin(SERIAL_BAUDRATE);
  
  // Wait a bit for serial to stabilize
  delay(100);
  
  Serial.println("[TRANS] Starting ESP-NOW Gateway Transmitter...");
  Serial.print("[TRANS] Device: ");
  Serial.println(WHO_AM_I);
}

bool readSerialMessage() {
  if (Serial.available() > 0) {
    // Clear buffer before reading to avoid leftover data
    memset(serialMessageBuffer, 0, SERIAL_BUFFER_SIZE);
    
    // Read the incoming message
    size_t bytesRead = Serial.readBytesUntil('\n', serialMessageBuffer, SERIAL_BUFFER_SIZE - 1);
    serialMessageBuffer[bytesRead] = '\0'; // Ensure null termination
    
    Serial.print("[TRANS] Message received from GW on serial: ");
    Serial.println(serialMessageBuffer);
    
    // Clear previous JSON document
    doc.clear();
    
    // Parse the message as JSON
    DeserializationError error = deserializeJson(doc, serialMessageBuffer);
    if (error) {
      Serial.print(F("[TRANS] deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }
    return true;
  }
  return false;
}

JsonDocument& getSerialDoc() {
  return doc;
}

// Handle command messages (ping, reset, etc.)
static void handleCommandMessage(const char* command) {
  if (strcmp(command, "ping") == 0) {
    Serial.print("[TRANS] PING response from ");
    Serial.print(WHO_AM_I);
    Serial.print(" - MAC: ");
    Serial.print(WiFi.macAddress());
    Serial.print(", Uptime: ");
    Serial.print(millis() / 1000);
    Serial.print("s, Peers: ");
    Serial.print(getEspNowPeerCount());
    Serial.print(", Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
  } 
  else if (strcmp(command, "reset") == 0) {
    Serial.println("[TRANS] RESET command received - rebooting device...");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
  else {
    Serial.print("[TRANS] ERROR: Unknown command: ");
    Serial.println(command);
  }
}

void handleSerialMessage() {
  // Check if this is a command message (ping, reset, etc.)
  if (doc.containsKey("command")) {
    const char* command = doc["command"];
    if (command != nullptr) {
      handleCommandMessage(command);
      return;
    }
  }
  
  // Validate required JSON fields for ESP-NOW messages
  if (!doc.containsKey("to")) {
    Serial.println("[TRANS] ERROR: Missing 'to' field in JSON");
    return;
  }
  
  if (!doc.containsKey("message")) {
    Serial.println("[TRANS] ERROR: Missing 'message' field in JSON");
    return;
  }
  
  const char* toField = doc["to"];
  if (toField == nullptr || strlen(toField) != 12) {
    Serial.println("[TRANS] ERROR: Invalid 'to' field - must be 12 hex characters");
    return;
  }
  
  // Get the message object and send via ESP-NOW
  JsonObject messageObj = doc["message"];
  if (messageObj.isNull()) {
    Serial.println("[TRANS] ERROR: 'message' field is not a valid object");
    return;
  }
  
  sendEspNowMessage(toField, messageObj);
}
