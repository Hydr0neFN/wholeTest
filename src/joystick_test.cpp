/*
 * joystick_test.cpp - ESP8266 Joystick Test
 * 
 * Hardware: ESP-12F on custom PCB
 * MAC Joystick 1: BC:FF:4D:F9:F3:91
 * MAC Joystick 2: BC:FF:4D:F9:AE:29
 * 
 * Tests:
 * - Button detection (GPIO14)
 * - Reaction timing
 * - ESP-NOW communication with Host
 * 
 * Pins:
 * - GPIO14: Button input
 * - GPIO4: SDA (MPU-6050) - NOT USED IN THIS TEST
 * - GPIO5: SCL (MPU-6050) - NOT USED IN THIS TEST
 * - GPIO12: Motor control - NOT USED IN THIS TEST
 * 
 * NOTE: Set MY_ID to ID_STICK1 or ID_STICK2 before uploading!
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "Protocol.h"
#include "GameTypes.h"

// =============================================================================
// CONFIGURATION - SET IN PLATFORMIO.INI
// =============================================================================
#ifndef MY_ID
#define MY_ID             ID_STICK1  // Default if not set by build flag
#endif

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#define PIN_BUTTON        14  // GPIO14

// =============================================================================
// ESP-NOW CONFIGURATION
// =============================================================================
uint8_t hostMac[6] = {0x88, 0x57, 0x21, 0xB3, 0x05, 0xAC};

// =============================================================================
// GAME STATE
// =============================================================================
GameState gameState = GAME_IDLE;
unsigned long gameStartTime = 0;
uint16_t reactionTime = 0;

// =============================================================================
// ESP-NOW SEND
// =============================================================================
void sendToHost(uint8_t cmd, uint16_t data) {
  GamePacket pkt;
  buildPacket(&pkt, ID_HOST, MY_ID, cmd, data);
  
  int result = esp_now_send(hostMac, (uint8_t*)&pkt, sizeof(pkt));
  
  if (result == 0) {
    Serial.printf("Sent CMD=0x%02X, DATA=%d\n", cmd, data);
  } else {
    Serial.printf("Send failed: %d\n", result);
  }
}

// =============================================================================
// ESP-NOW CALLBACKS
// =============================================================================
void OnDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(GamePacket)) return;
  
  GamePacket pkt;
  memcpy(&pkt, data, sizeof(pkt));
  
  if (!validatePacket(&pkt)) return;
  if (pkt.dest_id != MY_ID && pkt.dest_id != ID_BROADCAST) return;
  
  switch(pkt.cmd) {
    case CMD_IDLE:
      gameState = GAME_IDLE;
      reactionTime = 0;
      Serial.println("IDLE mode");
      break;
      
    case CMD_COUNTDOWN:
      gameState = GAME_COUNTDOWN;
      Serial.printf("Countdown: %d\n", pkt.data_low);
      break;
      
    case CMD_VIBRATE:
      if (pkt.data_low == VIBRATE_GO) {
        // Check for early press (button is active LOW, so check if it's already LOW)
        if (digitalRead(PIN_BUTTON) == LOW) {
          // Early press = penalty
          gameState = GAME_IDLE;
          reactionTime = TIME_PENALTY;
          sendToHost(CMD_REACTION_DONE, TIME_PENALTY);
          Serial.println("PENALTY - Early press!");
        } else {
          // Start timing
          gameState = GAME_REACTION_ACTIVE;
          gameStartTime = millis();
          Serial.println("GO! Waiting for button press...");
        }
      }
      break;
      
    default:
      break;
  }
}

void OnDataSent(uint8_t *mac, uint8_t status) {
  // Optional: track send status
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== JOYSTICK TEST (ESP8266) ===");
  
  // Button is active-low (uses INPUT_PULLUP)
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(ESPNOW_CHANNEL);
  
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  
  // Pair with Host
  int result = esp_now_add_peer(hostMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);
  
  if (result == 0) {
    Serial.println("Host paired");
  } else {
    Serial.printf("Host pair failed: %d\n", result);
  }
  
  Serial.printf("Joystick ID: 0x%02X\n", MY_ID);
  Serial.print("My MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Joystick ready!");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  // Handle button press during reaction phase
  if (gameState == GAME_REACTION_ACTIVE) {
    if (digitalRead(PIN_BUTTON) == LOW) {
      // Button pressed!
      reactionTime = (uint16_t)(millis() - gameStartTime);
      gameState = GAME_IDLE;
      
      sendToHost(CMD_REACTION_DONE, reactionTime);
      Serial.printf("Button pressed! Time: %d ms\n", reactionTime);
      
      delay(200); // Debounce
    }
    
    // Timeout check (10 seconds)
    if (millis() - gameStartTime > TIMEOUT_REACTION) {
      gameState = GAME_IDLE;
      reactionTime = TIME_PENALTY;
      sendToHost(CMD_REACTION_DONE, TIME_PENALTY);
      Serial.println("TIMEOUT!");
    }
  }
  
  delay(1);
}
