#include "logger.h"
#include "config.h"
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

// Forward declaration
static void handleCompletedLogLine(const String& line);

// Internal helper to buffer printed text into complete lines
static void bufferLogText(const String& text) {
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '\n') {
      // Clean up log message (only keep printable ASCII 32-126)
      String safeMsg = "";
      for (size_t j = 0; j < currentLineBuffer.length(); j++) {
        char ch = currentLineBuffer[j];
        if (ch >= 32 && ch <= 126) {
          safeMsg += ch;
        }
      }
      
      LogLine entry;
      entry.id = ++logCounter;
      entry.timestampMs = millis();
      entry.message = safeMsg;
      logBuffer.push_back(entry);
      if (logBuffer.size() > MAX_LOG_LINES) {
        logBuffer.pop_front();
      }
      
      handleCompletedLogLine(safeMsg);
      
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

void sendGatewayMessage(const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  uart2.println(out);
}

static void handleCompletedLogLine(const String& line) {
  String cleanMsg = line;
  
  // Skip empty lines
  if (cleanMsg.length() == 0) {
    return;
  }
  
  // Skip raw data messages if they are somehow passed here (safety check)
  if (cleanMsg.startsWith("DATA:")) {
    return;
  }
  
  String level = "info";
  String from = WHO_AM_I;
  
  // Check for Peer tag prefix [PEER:AABBCCDDEEFF]
  if (cleanMsg.startsWith("[PEER:")) {
    int closeBracket = cleanMsg.indexOf(']');
    if (closeBracket > 6) {
      from = cleanMsg.substring(6, closeBracket);
      cleanMsg = cleanMsg.substring(closeBracket + 1);
      cleanMsg.trim();
    }
  } else if (cleanMsg.startsWith("[TRANS] ")) {
    cleanMsg = cleanMsg.substring(8);
  } else if (cleanMsg.startsWith("[DRY RUN] ")) {
    cleanMsg = cleanMsg.substring(10);
    level = "debug";
  }
  
  // Check for level indicators in the message text
  if (cleanMsg.startsWith("ERROR: ")) {
    level = "error";
    cleanMsg = cleanMsg.substring(7);
  } else if (cleanMsg.startsWith("WARNING: ")) {
    level = "warning";
    cleanMsg = cleanMsg.substring(9);
  } else if (cleanMsg.indexOf("ERROR") >= 0 || cleanMsg.indexOf("failed") >= 0 || cleanMsg.indexOf("fail") >= 0) {
    level = "error";
  } else if (cleanMsg.indexOf("WARNING") >= 0) {
    level = "warning";
  }
  
  // Format log message as JSON and send to UART2 (gateway)
  JsonDocument doc;
  doc["type"] = "log";
  doc["from"] = from;
  doc["level"] = level;
  doc["message"] = cleanMsg;
  
  sendGatewayMessage(doc);
}

void logPrint(const char* message) {
  Serial.print(message);
  bufferLogText(message);
}

void logPrint(const String& message) {
  Serial.print(message);
  bufferLogText(message);
}

void logPrint(int value) {
  Serial.print(value);
  bufferLogText(String(value));
}

void logPrint(unsigned int value) {
  Serial.print(value);
  bufferLogText(String(value));
}

void logPrint(long value) {
  Serial.print(value);
  bufferLogText(String(value));
}

void logPrint(unsigned long value) {
  Serial.print(value);
  bufferLogText(String(value));
}

void logPrint(uint8_t value, int format) {
  Serial.print(value, format);
  bufferLogText(String(value, format));
}

void logPrintln(const char* message) {
  Serial.println(message);
  bufferLogText(String(message) + "\n");
}

void logPrintln(const String& message) {
  Serial.println(message);
  bufferLogText(message + "\n");
}

void logPrintln(int value) {
  Serial.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(unsigned int value) {
  Serial.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(long value) {
  Serial.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(unsigned long value) {
  Serial.println(value);
  bufferLogText(String(value) + "\n");
}

void logPrintln(uint8_t value, int format) {
  Serial.println(value, format);
  bufferLogText(String(value, format) + "\n");
}

void logPrintln() {
  Serial.println();
  bufferLogText("\n");
}

void logPrintf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  Serial.print(buffer);
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
