#include "logger.h"
#include <stdarg.h>

// UART2 configuration
#define UART2_TX_PIN 17
#define UART2_RX_PIN 16
#define UART2_BAUD 115200

// UART2 instance
static HardwareSerial uart2(2);

void setupLogger() {
  // Initialize USB Serial (UART0) for debugging
  Serial.begin(115200);
  
  // Initialize UART2 for MQTT module communication
  uart2.begin(UART2_BAUD, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  
  // Wait a bit for serial ports to stabilize
  delay(100);
}

void logPrint(const char* message) {
  Serial.print(message);
  uart2.print(message);
}

void logPrint(const String& message) {
  Serial.print(message);
  uart2.print(message);
}

void logPrint(int value) {
  Serial.print(value);
  uart2.print(value);
}

void logPrint(unsigned int value) {
  Serial.print(value);
  uart2.print(value);
}

void logPrint(long value) {
  Serial.print(value);
  uart2.print(value);
}

void logPrint(unsigned long value) {
  Serial.print(value);
  uart2.print(value);
}

void logPrint(uint8_t value, int format) {
  Serial.print(value, format);
  uart2.print(value, format);
}

void logPrintln(const char* message) {
  Serial.println(message);
  uart2.println(message);
}

void logPrintln(const String& message) {
  Serial.println(message);
  uart2.println(message);
}

void logPrintln(int value) {
  Serial.println(value);
  uart2.println(value);
}

void logPrintln(unsigned int value) {
  Serial.println(value);
  uart2.println(value);
}

void logPrintln(long value) {
  Serial.println(value);
  uart2.println(value);
}

void logPrintln(unsigned long value) {
  Serial.println(value);
  uart2.println(value);
}

void logPrintln(uint8_t value, int format) {
  Serial.println(value, format);
  uart2.println(value, format);
}

void logPrintln() {
  Serial.println();
  uart2.println();
}

void logPrintf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  Serial.print(buffer);
  uart2.print(buffer);
}

HardwareSerial& getUART2() {
  return uart2;
}
