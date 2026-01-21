#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Initialize serial communication
void setupSerial();

// Check for and read incoming serial messages
// Returns true if a valid message was received and parsed
bool readSerialMessage();

// Get the parsed JSON document
JsonDocument& getSerialDoc();

// Process the received serial message (commands or ESP-NOW messages)
void handleSerialMessage();

// Get peer count for status reporting
uint8_t getEspNowPeerCount();

#endif // SERIAL_HANDLER_H
