#include "serial_handler.h"
#include "espnow_handler.h"
#include "config.h"
#include "logger.h"
#include <WiFi.h>

#define SERIAL_BUFFER_SIZE 500

// Serial message buffer
static char serialMessageBuffer[SERIAL_BUFFER_SIZE];

// JSON document for parsed messages
static JsonDocument doc;

void setupSerial() {
  // Initialize logger (sets up both USB Serial and UART2)
  setupLogger();
  
  // Wait a bit for serial to stabilize
  delay(100);
  
  logPrintln("[TRANS] Starting ESP-NOW Gateway Transmitter...");
  logPrint("[TRANS] Device: ");
  logPrintln(WHO_AM_I);
}

bool readSerialMessage() {
  // Read from UART2 (MQTT module connection)
  HardwareSerial& uart = getUART2();
  
  if (uart.available() > 0) {
    // Clear buffer before reading to avoid leftover data
    memset(serialMessageBuffer, 0, SERIAL_BUFFER_SIZE);
    
    // Read the incoming message
    size_t bytesRead = uart.readBytesUntil('\n', serialMessageBuffer, SERIAL_BUFFER_SIZE - 1);
    serialMessageBuffer[bytesRead] = '\0'; // Ensure null termination
    
    logPrint("[TRANS] Message received from GW on serial: ");
    logPrintln(serialMessageBuffer);
    
    // Clear previous JSON document
    doc.clear();
    
    // Parse the message as JSON
    DeserializationError error = deserializeJson(doc, serialMessageBuffer);
    if (error) {
      logPrint(F("[TRANS] deserializeJson() failed: "));
      logPrintln(error.c_str());
      return false;
    }
    return true;
  }
  return false;
}

JsonDocument& getSerialDoc() {
  return doc;
}

// Handle command messages (ping, reset, set-mac, get-mac)
static void handleCommandMessage(const char* command) {
  if (strcmp(command, "ping") == 0) {
    logPrint("[TRANS] PING response from ");
    logPrint(WHO_AM_I);
    logPrint(" - MAC: ");
    logPrint(WiFi.macAddress());
    logPrint(", Uptime: ");
    logPrint(String(millis() / 1000));
    logPrint("s, Peers: ");
    logPrint(String(getEspNowPeerCount()));
    logPrint(", Free Heap: ");
    logPrint(String(ESP.getFreeHeap()));
    logPrintln(" bytes");
  } 
  else if (strcmp(command, "reset") == 0) {
    logPrintln("[TRANS] RESET command received - rebooting device...");
    Serial.flush();
    getUART2().flush();
    delay(100);
    ESP.restart();
  }
  else if (strcmp(command, "get-mac") == 0) {
    logPrint("[TRANS] Current MAC address: ");
    logPrintln(WiFi.macAddress());
  }
  else if (strcmp(command, "set-mac") == 0) {
    // Check if value field exists
    if (!doc.containsKey("value")) {
      logPrintln("[TRANS] ERROR: 'set-mac' command requires 'value' field");
      return;
    }
    
    const char* newMac = doc["value"];
    if (newMac == nullptr || strlen(newMac) != 12) {
      logPrintln("[TRANS] ERROR: MAC address must be 12 hex characters (e.g., 'AABBCCDDEEFF')");
      return;
    }
    
    // Convert hex string to byte array
    uint8_t macBytes[6];
    for (int i = 0; i < 6; i++) {
      char hex[3];
      hex[0] = newMac[i * 2];
      hex[1] = newMac[i * 2 + 1];
      hex[2] = '\0';
      macBytes[i] = strtol(hex, NULL, 16);
    }
    
    // Set the custom MAC address
    if (setCustomMacAddress(macBytes)) {
      logPrint("[TRANS] MAC address set to: ");
      for (int i = 0; i < 6; i++) {
        if (macBytes[i] < 16) logPrint("0");
        logPrint(String(macBytes[i], HEX));
      }
      logPrintln("");
      logPrintln("[TRANS] Rebooting to apply new MAC address...");
      Serial.flush();
      getUART2().flush();
      delay(100);
      ESP.restart();
    } else {
      logPrintln("[TRANS] ERROR: Failed to set MAC address");
    }
  }
  else {
    logPrint("[TRANS] ERROR: Unknown command: ");
    logPrintln(command);
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
    logPrintln("[TRANS] ERROR: Missing 'to' field in JSON");
    return;
  }
  
  if (!doc.containsKey("message")) {
    logPrintln("[TRANS] ERROR: Missing 'message' field in JSON");
    return;
  }
  
  const char* toField = doc["to"];
  if (toField == nullptr || strlen(toField) != 12) {
    logPrintln("[TRANS] ERROR: Invalid 'to' field - must be 12 hex characters");
    return;
  }
  
  // Get the message object and send via ESP-NOW
  JsonObject messageObj = doc["message"];
  if (messageObj.isNull()) {
    logPrintln("[TRANS] ERROR: 'message' field is not a valid object");
    return;
  }
  
  sendEspNowMessage(toField, messageObj);
}
