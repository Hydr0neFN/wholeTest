/*
 * host_test.cpp - ESP32 Host Test
 * 
 * Hardware: ESP32 DevKit-C (ZY-ESP32)
 * MAC: 88:57:21:B3:05:AC
 * 
 * Tests:
 * - ESP-NOW broadcast to Display + 2 Joysticks
 * - Audio playback (countdown + GO beep)
 * - NeoPixel animations (5 rings)
 * - Game timing logic
 * 
 * Pins:
 * - GPIO4: NeoPixel DIN
 * - GPIO25: I2S DOUT (WiFi disabled during audio playback)
 * - GPIO26: I2S BCLK (WiFi disabled during audio playback)
 * - GPIO27: I2S LRC (WiFi disabled during audio playback)
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>
#include "Protocol.h"
#include "GameTypes.h"
#include "AudioManager.h"

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#define PIN_NEOPIXEL      4

// =============================================================================
// ESP-NOW CONFIGURATION
// =============================================================================
uint8_t displayMac[6] = {0xD0, 0xCF, 0x13, 0x01, 0xD1, 0xA4};
uint8_t stick1Mac[6]  = {0xBC, 0xFF, 0x4D, 0xF9, 0xF3, 0x91};
uint8_t stick2Mac[6]  = {0xBC, 0xFF, 0x4D, 0xF9, 0xAE, 0x29};
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// =============================================================================
// HARDWARE
// =============================================================================
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
AudioManager audio;

// =============================================================================
// GAME STATE
// =============================================================================
GameState gameState = GAME_IDLE;
Player players[2]; // Only 2 joysticks for test
unsigned long stateStartTime = 0;
uint8_t countdownNum = 3;
NeoMode neoMode = NEO_OFF;

// =============================================================================
// NEOPIXEL HELPERS
// =============================================================================
void setRingColor(uint8_t ring, uint32_t color) {
  uint8_t start = ring * LEDS_PER_RING;
  for (uint8_t i = 0; i < LEDS_PER_RING; i++) {
    pixels.setPixelColor(start + i, color);
  }
}

void setAllRings(uint32_t color) {
  for (int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, color);
  }
}

uint32_t wheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) return pixels.Color(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85; return pixels.Color(0, pos * 3, 255 - pos * 3); }
  pos -= 170;
  return pixels.Color(pos * 3, 255 - pos * 3, 0);
}

void updateNeoPixels() {
  static uint8_t offset = 0;
  static unsigned long lastUpdate = 0;
  static bool blinkState = false;
  
  unsigned long now = millis();
  
  switch (neoMode) {
    case NEO_OFF:
      setAllRings(0);
      break;
      
    case NEO_IDLE_RAINBOW:
      if (now - lastUpdate > 50) {
        lastUpdate = now;
        for (int i = 0; i < NEOPIXEL_COUNT; i++) {
          pixels.setPixelColor(i, wheel((i * 256 / NEOPIXEL_COUNT + offset) & 255));
        }
        offset++;
      }
      break;
      
    case NEO_STATUS:
      // Player 1 = Ring 0, Player 2 = Ring 1, Center = Ring 2
      setRingColor(0, players[0].joined ? COLOR_GREEN : COLOR_RED);
      setRingColor(1, players[1].joined ? COLOR_GREEN : COLOR_RED);
      setRingColor(2, wheel(offset++));
      setRingColor(3, 0); // Ring 3, 4 off
      setRingColor(4, 0);
      break;
      
    case NEO_COUNTDOWN:
      if (now - lastUpdate > 250) {
        lastUpdate = now;
        blinkState = !blinkState;
        setAllRings(blinkState ? COLOR_RED : 0);
      }
      break;
      
    case NEO_FIXED_COLOR:
      setAllRings(COLOR_GREEN); // GO signal
      break;
      
    default:
      break;
  }
  
  pixels.show();
}

// =============================================================================
// ESP-NOW SEND
// =============================================================================
void sendPacket(uint8_t* mac, uint8_t dest, uint8_t cmd, uint16_t data) {
  GamePacket pkt;
  buildPacket(&pkt, dest, ID_HOST, cmd, data);
  esp_now_send(mac, (uint8_t*)&pkt, sizeof(pkt));
}

void broadcast(uint8_t cmd, uint16_t data) {
  sendPacket(broadcastMac, ID_BROADCAST, cmd, data);
}

// =============================================================================
// ESP-NOW CALLBACKS
// =============================================================================
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(GamePacket)) return;

  GamePacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (!validatePacket(&pkt)) return;

  // Handle joystick responses
  if (pkt.cmd == CMD_REACTION_DONE) {
    uint8_t playerIdx = (pkt.src_id == ID_STICK1) ? 0 : 1;
    players[playerIdx].reactionTime = packetData(&pkt);
    players[playerIdx].finished = true;

    Serial.printf("Player %d: %d ms\n", playerIdx + 1, players[playerIdx].reactionTime);

    // Update NeoPixel ring
    uint32_t color = (players[playerIdx].reactionTime == TIME_PENALTY) ? COLOR_RED : COLOR_GREEN;
    setRingColor(playerIdx, color);
    pixels.show();
  }
}

void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  // Optional: track send failures
}

// =============================================================================
// GAME STATE MACHINE
// =============================================================================
void runGame() {
  unsigned long now = millis();
  
  switch (gameState) {
    case GAME_IDLE:
      if (stateStartTime == 0) {
        stateStartTime = now;
        neoMode = NEO_IDLE_RAINBOW;
        
        // Initialize players
        players[0] = {true, false, 0, 0};
        players[1] = {true, false, 0, 0};

        broadcast(CMD_IDLE, 0);
        audio.queueSound(SND_GET_READY);
        
        Serial.println("IDLE - Press button to start");
      }
      
      // Auto-start after 3 seconds
      if (now - stateStartTime > 3000) {
        gameState = GAME_COUNTDOWN;
        stateStartTime = 0;
        countdownNum = 3;
      }
      break;
      
    case GAME_COUNTDOWN:
      if (stateStartTime == 0) {
        stateStartTime = now;
        neoMode = NEO_COUNTDOWN;
        audio.playCountdown(countdownNum);
        broadcast(CMD_COUNTDOWN, countdownNum);
        
        Serial.printf("Countdown: %d\n", countdownNum);
      }
      
      if (now - stateStartTime > 1000) {
        countdownNum--;
        if (countdownNum > 0) {
          stateStartTime = now;
          audio.playCountdown(countdownNum);
          broadcast(CMD_COUNTDOWN, countdownNum);
          Serial.printf("Countdown: %d\n", countdownNum);
        } else {
          gameState = GAME_REACTION_ACTIVE;
          stateStartTime = 0;
        }
      }
      break;
      
    case GAME_REACTION_ACTIVE: {
      if (stateStartTime == 0) {
        stateStartTime = now;
        neoMode = NEO_FIXED_COLOR;

        // Send GO signal
        broadcast(CMD_VIBRATE, VIBRATE_GO);
        audio.queueSound(SND_BEEP);

        Serial.println("GO!");
      }

      // Check if both finished or timeout
      bool allDone = players[0].finished && players[1].finished;
      bool timeout = (now - stateStartTime > TIMEOUT_REACTION);

      if (allDone || timeout) {
        gameState = GAME_RESULTS;
        stateStartTime = 0;
      }
      break;
    }
      
    case GAME_RESULTS:
      if (stateStartTime == 0) {
        stateStartTime = now;
        neoMode = NEO_STATUS;
        
        Serial.println("\n=== RESULTS ===");
        Serial.printf("Player 1: %d ms\n", players[0].reactionTime);
        Serial.printf("Player 2: %d ms\n", players[1].reactionTime);
        
        // Determine winner
        if (players[0].reactionTime < players[1].reactionTime && 
            players[0].reactionTime != TIME_PENALTY) {
          Serial.println("Player 1 WINS!");
          audio.queueSound(SND_VICTORY_FANFARE);
        } else if (players[1].reactionTime < players[0].reactionTime && 
                   players[1].reactionTime != TIME_PENALTY) {
          Serial.println("Player 2 WINS!");
          audio.queueSound(SND_VICTORY_FANFARE);
        } else {
          Serial.println("TIE or BOTH PENALTY");
        }
      }
      
      // Return to IDLE after 5 seconds
      if (now - stateStartTime > 5000) {
        gameState = GAME_IDLE;
        stateStartTime = 0;
      }
      break;
      
    default:
      break;
  }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== HOST TEST (ESP32) ===");

  // Initialize Audio FIRST (before WiFi/ESP-NOW)
  if (audio.begin(1.0)) {
    Serial.println("Audio system ready");
  } else {
    Serial.println("Audio init failed!");
  }

  // Initialize NeoPixels
  pixels.begin();
  pixels.setBrightness(NEO_BRIGHTNESS);
  pixels.show();

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  // Add peers
  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  // Broadcast
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  esp_now_add_peer(&peerInfo);

  // Display
  memcpy(peerInfo.peer_addr, displayMac, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("Display paired");
  }

  // Joystick 1
  memcpy(peerInfo.peer_addr, stick1Mac, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("Joystick 1 paired");
  }

  // Joystick 2
  memcpy(peerInfo.peer_addr, stick2Mac, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("Joystick 2 paired");
  }

  Serial.print("Host MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Host ready!");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  audio.update(); // Non-blocking audio
  updateNeoPixels();
  runGame();
  delay(1);
}
