#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// Initialize logging system (both USB Serial and UART2)
void setupLogger();

// Log a message to both USB and UART2
void logPrint(const char* message);
void logPrint(const String& message);
void logPrint(int value);
void logPrint(unsigned int value);
void logPrint(long value);
void logPrint(unsigned long value);
void logPrint(uint8_t value, int format = DEC); // Support HEX format

void logPrintln(const char* message);
void logPrintln(const String& message);
void logPrintln(int value);
void logPrintln(unsigned int value);
void logPrintln(long value);
void logPrintln(unsigned long value);
void logPrintln(uint8_t value, int format = DEC); // Support HEX format
void logPrintln(); // Empty line

// Printf-style logging
void logPrintf(const char* format, ...);

// Get direct access to UART2 for reading incoming messages
HardwareSerial& getUART2();

#endif // LOGGER_H
