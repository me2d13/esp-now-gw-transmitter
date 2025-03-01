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
    // read the incoming byte:
    Serial.readBytesUntil('\n', serialMessageBuffer, SERIAL_BUFFER_SIZE);
    Serial.print("Message received from serial: ");
    Serial.println(serialMessageBuffer);
    // parse the message as json
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, serialMessageBuffer);
    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }
    return true;
  }
  return false;
}

// Callback when data is sent
void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last espnow send status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}

void blickTimes(int times)
{
  for (int i = 0; i < times; i++)
  {
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
  }
}

void onDataReceived(unsigned char* mac, unsigned char *incomingData, u8 len) {
  memcpy(espNowMessageBuffer, incomingData, len);
  Serial.print("Bytes received ");
  Serial.print(len);
  Serial.print(": ");
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
  setupCrypto();

  // Init Serial Monitor
  Serial.swap(); // swap serial pins to use GPIO13 as RX and GPIO15 as TX
  Serial.begin(SERIAL_BAUDRATE);
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("ESP-NOW transmitter started");
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

void handleSerialMessage() {
  uint8_t peerAddress[6];
  // convert "to" element from json to byte array
  charToByteArray(doc["to"], peerAddress);
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
    memcpy(peerList[peerCount], peerAddress, 6);
    peerCount++;
    esp_now_add_peer(peerAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    Serial.println("New peer added");
  }
  // take message element from json as nested object and send it to the peer via esp_now_send
  JsonObject data = doc["message"];
  // convert nested object to string
  String dataStr;
  serializeJson(data, dataStr);
  uint8_t dataBytes[250];

  int length = messageToByteArray(dataStr.c_str(), dataBytes, ENABLE_ENCRYPTION);
  if (length > 0) {
    Serial.print("Sending ");
    logMessageToSerial(dataBytes, length, ENABLE_ENCRYPTION);
    esp_now_send(peerAddress, dataBytes, length);
  }
}

void loop() {
  // get current millis
  unsigned long currentMillis = millis();
  // blick led 2 times every 10 seconds in non-blocking way
  if (currentMillis - lastBlickMilis >= 10000)
  {
    lastBlickMilis = currentMillis;
    blickTimes(1);
  }
  // handle heartbeat
  if (currentMillis - lastHeartbeatMessageMilis >= HEART_BEAT_S * 1000)
  {
    lastHeartbeatMessageMilis = currentMillis;
    Serial.println("Heartbeet from espnow transmitter");
  }
  if (readMessageFromSerial())
  {
    handleSerialMessage();
  }
}
