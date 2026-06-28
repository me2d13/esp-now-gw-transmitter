#include "logger.h"
#include <stdarg.h>
#include <deque>
#include <ArduinoJson.h>

// UART2 configuration
#define UART2_TX_PIN 17
#define UART2_RX_PIN 16
#define UART2_BAUD 115200

// UART2 instance
static HardwareSerial uart2(2);

// Circular log buffer structures
struct LogLine {
  uint32_t id;
  unsigned long timestampMs;
  String message;
};

static std::deque<LogLine> logBuffer;
static uint32_t logCounter = 0;
static String currentLineBuffer = "";
static const size_t MAX_LOG_LINES = 100;

// Internal helper to buffer printed text into complete lines
static void bufferLogText(const String& text) {
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '\n') {
      LogLine entry;
      entry.id = ++logCounter;
      entry.timestampMs = millis();
      entry.message = currentLineBuffer;
      logBuffer.push_back(entry);
      if (logBuffer.size() > MAX_LOG_LINES) {
        logBuffer.pop_front();
      }
      currentLineBuffer = "";
    } else if (c != '\r') {
      currentLineBuffer += c;
    }
  }
}

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
  bufferLogText(message);
}

void logPrint(const String& message) {
  Serial.print(message);
  uart2.print(message);
  bufferLogText(message);
}

void logPrint(int value) {
  Serial.print(value);
  uart2.print(value);
  bufferLogText(String(value));
}

void logPrint(unsigned int value) {
  Serial.print(value);
  uart2.print(value);
  bufferLogText(String(value));
}

void logPrint(long value) {
  Serial.print(value);
  uart2.print(value);
  bufferLogText(String(value));
}

void logPrint(unsigned long value) {
  Serial.print(value);
  uart2.print(value);
  bufferLogText(String(value));
}

void logPrint(uint8_t value, int format) {
  Serial.print(value, format);
  uart2.print(value, format);
  bufferLogText(String(value, format));
}

void logPrintln(const char* message) {
  Serial.println(message);
  uart2.println(message);
  bufferLogText(String(message) + "\n");
}

void logPrintln(const String& message) {
  Serial.println(message);
  uart2.println(message);
  bufferLogText(message + "\n");
}

void logPrintln(int value) {
  Serial.println(value);
  uart2.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(unsigned int value) {
  Serial.println(value);
  uart2.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(long value) {
  Serial.println(value);
  uart2.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(unsigned long value) {
  Serial.println(value);
  uart2.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(uint8_t value, int format) {
  Serial.println(value, format);
  uart2.println(value, format);
  bufferLogText(String(value, format) + "\n");
}

void logPrintln() {
  Serial.println();
  uart2.println();
  bufferLogText("\n");
}

void logPrintf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  Serial.print(buffer);
  uart2.print(buffer);
  bufferLogText(buffer);
}

HardwareSerial& getUART2() {
  return uart2;
}

String getLogsJson() {
  JsonDocument doc;
  JsonArray logArray = doc.to<JsonArray>();
  
  for (const auto& log : logBuffer) {
    JsonObject obj = logArray.add<JsonObject>();
    obj["id"] = log.id;
    
    // Format timestamp as HH:MM:SS.mmm
    unsigned long totalMillis = log.timestampMs;
    unsigned long hours = totalMillis / 3600000;
    unsigned long minutes = (totalMillis % 3600000) / 60000;
    unsigned long seconds = (totalMillis % 60000) / 1000;
    unsigned long ms = totalMillis % 1000;
    
    char timeBuf[32];
    sprintf(timeBuf, "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds, ms);
    
    obj["timestamp"] = String(timeBuf);
    obj["message"] = log.message;
    obj["job"] = "TRANS";
  }
  
  String response;
  serializeJson(doc, response);
  return response;
}

void clearLogBuffer() {
  logBuffer.clear();
}
