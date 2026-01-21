#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ArduinoJson.h>
#include "config.h"
#include "crypto.h"

#define SERIAL_BUFFER_SIZE 500
#define SERIAL_BAUDRATE 115200

#define ENABLE_ENCRIPTION 1

byte ledPin = LED_BUILTIN;
bool ledState = HIGH;
unsigned long ledToggleTime = 0;
int ledBlinkCount = 0;
int ledBlinkTarget = 0;

// Software watchdog
unsigned long lastLoopTime = 0;

// 20x6 bytes array to store the mac address of the peers
uint8_t peerList[20][6];
uint8_t peerCount = 0;

char serialMessageBuffer[SERIAL_BUFFER_SIZE];
byte espNowMessageBuffer[251];

// Allocate the JSON document
JsonDocument doc;

// function to listen on serial port for incoming messages
// when message is received, parse it as json and return parsed message
bool readMessageFromSerial()
{
  if (Serial.available() > 0)
  {
    // Clear buffer before reading to avoid leftover data
    memset(serialMessageBuffer, 0, SERIAL_BUFFER_SIZE);
    
    // read the incoming byte:
    size_t bytesRead = Serial.readBytesUntil('\n', serialMessageBuffer, SERIAL_BUFFER_SIZE - 1);
    serialMessageBuffer[bytesRead] = '\0'; // Ensure null termination
    
    Serial.print("[TRANS] Message received from GW on serial: ");
    Serial.println(serialMessageBuffer);
    
    // Clear previous JSON document
    doc.clear();
    
    // parse the message as json
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, serialMessageBuffer);
    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("[TRANS] deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }
    return true;
  }
  return false;
}

// Callback when data is sent
void onEspNowDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("[TRANS] Last espnow send status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}

// Non-blocking LED blink - call this from loop()
void updateLedBlink()
{
  if (ledBlinkTarget > 0)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - ledToggleTime >= 100)
    {
      ledToggleTime = currentMillis;
      ledState = !ledState;
      digitalWrite(ledPin, ledState);
      
      if (!ledState) // Count on LOW transitions
      {
        ledBlinkCount++;
        if (ledBlinkCount >= ledBlinkTarget)
        {
          ledBlinkTarget = 0;
          ledBlinkCount = 0;
          ledState = HIGH;
          digitalWrite(ledPin, HIGH);
        }
      }
    }
  }
}

// Start a non-blocking blink sequence
void startBlink(int times)
{
  ledBlinkTarget = times;
  ledBlinkCount = 0;
  ledToggleTime = millis();
}

void onEspNowDataReceived(unsigned char* mac, unsigned char *incomingData, u8 len) {
  memcpy(espNowMessageBuffer, incomingData, len);
  Serial.print("[TRANS] From esp-now received ");
  Serial.print(len);
  Serial.print(" bytes: ");
  logMessageToSerial(incomingData, len, ENABLE_ENCRYPTION);
  Serial.print("DATA:{\"mac\":\"");
  for (int i = 0; i < 6; i++)
  {
    Serial.print(mac[i], HEX);
  }
  Serial.print("\",\"message\":");
  if (ENABLE_ENCRYPTION) {
    inPlaceDecrypt(espNowMessageBuffer, len);
  }
  Serial.print((char *) espNowMessageBuffer);
  Serial.println("}");
}

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  
  bool initSuccess = true;
  unsigned long initStartTime = millis();
  
  setupCrypto();

  // Init Serial Monitor
  Serial.swap(); // swap serial pins to use GPIO13 as RX and GPIO15 as TX
  Serial.begin(SERIAL_BAUDRATE);
  
  // Wait a bit for serial to stabilize
  delay(100);
  
  Serial.println("[TRANS] Starting ESP-NOW Gateway Transmitter...");
  Serial.print("[TRANS] Device: ");
  Serial.println(WHO_AM_I);
  
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
    while (millis() - initStartTime < INIT_RETRY_TIMEOUT_MS) {
      digitalWrite(ledPin, LOW);
      delay(100);
      digitalWrite(ledPin, HIGH);
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
  
  // Initialize watchdog
  lastLoopTime = millis();
}

unsigned long lastBlickMilis = 0;
unsigned long lastHeartbeatMessageMilis = 0;

// function which converts char* with hex bytes to byte array
void charToByteArray(const char* charArray, uint8_t* byteArray)
{
  for (int i = 0; i < 6; i++)
  {
    char hex[3];
    hex[0] = charArray[i * 2];
    hex[1] = charArray[i * 2 + 1];
    hex[2] = '\0';
    byteArray[i] = strtol(hex, NULL, 16);
  }
}


// Handle command messages - ping and reset
void handleCommandMessage(const char* command) {
  if (strcmp(command, "ping") == 0) {
    Serial.print("[TRANS] PING response from ");
    Serial.print(WHO_AM_I);
    Serial.print(" - MAC: ");
    Serial.print(WiFi.macAddress());
    Serial.print(", Uptime: ");
    Serial.print(millis() / 1000);
    Serial.print("s, Peers: ");
    Serial.print(peerCount);
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
  
  uint8_t peerAddress[6];
  // convert "to" element from json to byte array
  charToByteArray(toField, peerAddress);
  
  // if peerList doesn't contain the peer, add it
  bool found = false;
  for (int i = 0; i < peerCount; i++)
  {
    if (memcmp(peerList[i], peerAddress, 6) == 0)
    {
      found = true;
      break;
    }
  }
  
  if (!found)
  {
    if (peerCount >= 20) {
      Serial.println("[TRANS] ERROR: Peer list full (max 20 peers)");
      return;
    }
    
    memcpy(peerList[peerCount], peerAddress, 6);
    peerCount++;
    
    int addPeerResult = esp_now_add_peer(peerAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    if (addPeerResult != 0) {
      Serial.print("[TRANS] ERROR: Failed to add peer, code: ");
      Serial.println(addPeerResult);
      peerCount--; // Rollback
      return;
    }
    Serial.println("[TRANS] New peer added");
  }
  
  // take message element from json as nested object and send it to the peer via esp_now_send
  JsonObject data = doc["message"];
  if (data.isNull()) {
    Serial.println("[TRANS] ERROR: 'message' field is not a valid object");
    return;
  }
  
  // convert nested object to string
  String dataStr;
  serializeJson(data, dataStr);
  
  if (dataStr.length() == 0) {
    Serial.println("[TRANS] ERROR: Empty message");
    return;
  }
  
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

void loop() {
  // get current millis
  unsigned long currentMillis = millis();
  
  // Software watchdog - check if loop is running
  if (currentMillis - lastLoopTime > WATCHDOG_TIMEOUT_S * 1000UL) {
    Serial.println("[TRANS] ERROR: Watchdog timeout - system appears hung");
    Serial.println("[TRANS] Rebooting...");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
  lastLoopTime = currentMillis();
  
  // Update non-blocking LED blink
  updateLedBlink();
  
  // blink led 1 time every 10 seconds in non-blocking way
  if (currentMillis - lastBlickMilis >= 10000)
  {
    lastBlickMilis = currentMillis;
    startBlink(1);
  }
  
  // handle heartbeat
  if (currentMillis - lastHeartbeatMessageMilis >= HEART_BEAT_S * 1000UL)
  {
    lastHeartbeatMessageMilis = currentMillis;
    Serial.print("[TRANS] Heartbeat from ");
    Serial.print(WHO_AM_I);
    Serial.print(" - Uptime: ");
    Serial.print(currentMillis / 1000);
    Serial.print("s, Peers: ");
    Serial.println(peerCount);
  }
  
  // Handle incoming serial messages
  if (readMessageFromSerial())
  {
    handleSerialMessage();
  }
  
  // Small yield to prevent watchdog issues
  yield();
}
