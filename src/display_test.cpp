/*
 * display_test.cpp - ESP32-S3 Display Test
 *
 * Hardware: ESP32-S3 Touch LCD 7" (800x480)
 * MAC: D0:CF:13:01:D1:A4
 *
 * Tests:
 * - LVGL UI (player circles, GO text, reaction times)
 * - ESP-NOW reception from Host
 * - Embedded bitmap images (compiled into firmware)
 *
 * Pin usage: RGB parallel display (handled by lgfx_conf)
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "lvgl.h"
#include "ui_lib.h"  // Triggers PlatformIO LDF to compile lib/ui
#include "Protocol.h"

// =============================================================================
// ESP-NOW CONFIGURATION
// =============================================================================
uint8_t hostMac[6] = {0x88, 0x57, 0x21, 0xB3, 0x05, 0xAC};

// =============================================================================
// GAME STATE
// =============================================================================
enum DisplayState {
  DISP_IDLE,
  DISP_COUNTDOWN,
  DISP_GO_SIGNAL,
  DISP_RESULTS
};

DisplayState currentState = DISP_IDLE;
uint8_t countdownValue = 0;
uint16_t playerTimes[4] = {0, 0, 0, 0};  // Support 4 players
uint8_t activePlayers = 2;  // Default to 2 for testing

// =============================================================================
// LVGL UI HELPERS
// =============================================================================
void showPlayerCircle(uint8_t player, bool active) {
  lv_obj_t* circle = nullptr;
  
  switch(player) {
    case 1: circle = ui_player1; break;
    case 2: circle = ui_player2; break;
    case 3: circle = ui_player3; break;
    case 4: circle = ui_player4; break;
    default: return;
  }
  
  if (active) {
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(circle, LV_OBJ_FLAG_HIDDEN);
  }
}

void showCountdown(uint8_t num) {
  // Hide GO and START
  lv_obj_add_flag(ui_imgGo, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_imgStart, LV_OBJ_FLAG_HIDDEN);
  
  // TODO: Add countdown numbers (3, 2, 1) as labels or images
  // For now just show center circle pulsing
  lv_obj_clear_flag(ui_centerCircle, LV_OBJ_FLAG_HIDDEN);
  
  Serial.printf("COUNTDOWN: %d\n", num);
}

void showGO() {
  lv_obj_add_flag(ui_imgStart, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_imgGo, LV_OBJ_FLAG_HIDDEN);
  
  Serial.println("GO!");
}

void showResults(uint16_t times[4]) {
  lv_obj_add_flag(ui_imgGo, LV_OBJ_FLAG_HIDDEN);
  
  // TODO: Add time labels under player circles
  // For now just print to serial
  Serial.println("RESULTS:");
  for (uint8_t i = 0; i < activePlayers; i++) {
    Serial.printf("  Player %d: %d ms\n", i + 1, times[i]);
  }
  
  // Find winner (lowest valid time)
  uint8_t winner = 0xFF;
  uint16_t bestTime = 0xFFFF;
  for (uint8_t i = 0; i < activePlayers; i++) {
    if (times[i] < bestTime && times[i] != 0xFFFF) {
      bestTime = times[i];
      winner = i;
    }
  }
  
  if (winner != 0xFF) {
    Serial.printf("Player %d WINS!\n", winner + 1);
  } else {
    Serial.println("NO WINNER (all penalties)");
  }
}

// =============================================================================
// ESP-NOW CALLBACKS
// =============================================================================
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(GamePacket)) return;

  GamePacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (!validatePacket(&pkt)) return;
  if (pkt.dest_id != ID_HOST && pkt.dest_id != ID_BROADCAST) return; // Only process if for display
  
  switch(pkt.cmd) {
    case CMD_COUNTDOWN:
      countdownValue = pkt.data_low;
      currentState = DISP_COUNTDOWN;
      showCountdown(countdownValue);
      break;
      
    case CMD_VIBRATE:
      if (pkt.data_low == VIBRATE_GO) {
        currentState = DISP_GO_SIGNAL;
        showGO();
      }
      break;
      
    case CMD_REACTION_DONE:
      // Store reaction time for any player (1-4)
      if (pkt.src_id >= ID_STICK1 && pkt.src_id <= ID_STICK4) {
        uint8_t playerIdx = pkt.src_id - ID_STICK1;  // Convert to 0-3 index
        playerTimes[playerIdx] = packetData(&pkt);
        Serial.printf("Player %d done: %d ms\n", playerIdx + 1, playerTimes[playerIdx]);
        
        // Check if all active players finished
        bool allDone = true;
        for (uint8_t i = 0; i < activePlayers; i++) {
          if (playerTimes[i] == 0) {
            allDone = false;
            break;
          }
        }
        
        if (allDone) {
          currentState = DISP_RESULTS;
          showResults(playerTimes);
        }
      }
      break;
      
    case CMD_IDLE:
      currentState = DISP_IDLE;
      for (uint8_t i = 0; i < 4; i++) {
        playerTimes[i] = 0;
      }
      lv_obj_add_flag(ui_imgGo, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_imgStart, LV_OBJ_FLAG_HIDDEN);
      Serial.println("IDLE mode");
      break;
  }
}

void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  // Display doesn't send, only receives
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== DISPLAY TEST (ESP32-S3) ===");
  
  
  // Initialize LVGL + Display
  lv_init();
  display_init();
  
  // Show all 4 players (even if only 2 joysticks for test)
  for (uint8_t i = 1; i <= 4; i++) {
    showPlayerCircle(i, true);
  }
  lv_obj_clear_flag(ui_imgStart, LV_OBJ_FLAG_HIDDEN); // Show START initially
  
  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  
  // Pair with Host
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, hostMac, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("Host paired");
  } else {
    Serial.println("Host pair failed!");
  }
  
  Serial.print("Display MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Display ready!");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  lv_timer_handler();
  delay(5);
}
