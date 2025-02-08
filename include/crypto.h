#ifndef CRYPTO_H
#define CRYPTO_H
#include <Arduino.h>

void setupCrypto();
void aes_decrypt(uint8_t* encryptedMsg, int encryptedLen, char* decryptedText);
int aes_encrypt(const char* plainText, uint8_t* encryptedMsg, int maxSize);
void logMessageToSerial(byte* message, int length, bool isEncrypted);
int messageToByteArray(const char* charArray, byte* byteArray, bool encrypt, int maxLength = 250);
void inPlaceDecryptAndLog(byte* message, int length);
void inPlaceDecrypt(byte* message, int length);

#endif // CRYPOT_H
