#include "crypto.h"
#include <Arduino.h>
#include "config.h"
#include "aes.hpp"  // tiny-AES library


byte key[32] = CRYPTO_KEY;

void setupCrypto() {
    randomSeed(analogRead(0));  // Seed the random number generator
}

void generateRandomIV(uint8_t* iv, size_t length) {
    for (size_t i = 0; i < length; i++) {
        iv[i] = random(0, 256);  // Get a random byte (0-255)
    }
}

// **AES-CTR Decrypt (Extract IV & Ciphertext)**
void aes_decrypt(uint8_t* encryptedMsg, int encryptedLen, char* decryptedText) {
    if (encryptedLen < 16) return;

    uint8_t iv[16];
    memcpy(iv, encryptedMsg, 16);  // Extract IV

    int cipherLen = encryptedLen - 16;
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CTR_xcrypt_buffer(&ctx, encryptedMsg + 16, cipherLen);

    memcpy(decryptedText, encryptedMsg + 16, cipherLen);
    decryptedText[cipherLen] = '\0';  // Null-terminate string
}

// **AES-CTR Encrypt and Pack (IV + Ciphertext)**
int aes_encrypt(const char* plainText, uint8_t* encryptedMsg, int maxSize) {
    int len = strlen(plainText);
    if (len + 16 > maxSize) return -1;  // Ensure it fits within ESP-NOW size

    uint8_t iv[16];
    generateRandomIV(iv, 16);

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CTR_xcrypt_buffer(&ctx, (uint8_t*)plainText, len);

    // **Pack IV + Ciphertext into one message**
    memcpy(encryptedMsg, iv, 16);
    memcpy(encryptedMsg + 16, plainText, len);

    return len + 16;  // Total size (IV + Ciphertext)
}

void logMessageToSerial(byte* message, int length, bool isEncrypted) {
    if (isEncrypted) {
        Serial.print("Encrypted message ");
        Serial.print(length);
        Serial.print(" bytes: ");
        for (int i = 0; i < length; i++) {
            Serial.print(message[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.print("Plain message: ");
        Serial.println((char*)message);
    }
}

int messageToByteArray(const char* charArray, byte* byteArray, bool encrypt, int maxLength) {
    int length = strlen(charArray);
    if (encrypt) {
        int encryptedLen = aes_encrypt(charArray, byteArray, maxLength);
        if (encryptedLen == -1) {
            Serial.print("Message too long - ");
            Serial.print(length);
            Serial.print(", max ");
            Serial.print(maxLength-16);
            Serial.println(" bytes allowed when encrypted");
            return -1;
        }
        return encryptedLen;
    } else {
        if (length > maxLength) {
            Serial.print("Message too long - ");
            Serial.print(length);
            Serial.print(", max ");
            Serial.print(maxLength);
            Serial.println(" bytes allowed");
            return -1;
        }
        memcpy(byteArray, charArray, length);
        byteArray[length] = '\0';
        return length+1;
    }
}

void inPlaceDecryptAndLog(byte* message, int length) {
    char decryptedText[251];
    aes_decrypt(message, length, decryptedText);
    //Serial.print("Decrypted message: ");
    //Serial.println(decryptedText);
    length = strlen(decryptedText);
    logMessageToSerial((byte*)decryptedText, length, false);
    memcpy(message, decryptedText, length+1); // Copy null-terminator
}

void inPlaceDecrypt(byte* message, int length) {
    char decryptedText[251];
    aes_decrypt(message, length, decryptedText);
    length = strlen(decryptedText);
    memcpy(message, decryptedText, length+1); // Copy null-terminator
}