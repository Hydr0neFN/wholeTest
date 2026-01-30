# Reaction Game - Test Programs

Simplified 3-device test to validate all hardware before building the full game.

## Hardware Setup

### 1. Display ESP32-S3 (800x480 Touch LCD)
- **MAC**: `D0:CF:13:01:D1:A4`
- **Role**: Receives ESP-NOW packets, shows UI
- **Pins**: RGB parallel display (auto-configured)

### 2. Host ESP32 (DevKit-C / ZY-ESP32)
- **MAC**: `88:57:21:B3:05:AC`
- **Role**: Game logic, audio, NeoPixels, ESP-NOW coordinator
- **Pins**:
  - GPIO4: NeoPixel DIN (5 rings × 12 LEDs)
  - GPIO23: I2S DOUT (to MAX98357A)
  - GPIO26: I2S BCLK
  - GPIO25: I2S LRC

### 3. Joystick ESP8266 (ESP-12F) × 2
- **MAC Joystick 1**: `BC:FF:4D:F9:F3:91`
- **MAC Joystick 2**: `BC:FF:4D:F9:AE:29`
- **Role**: Button input, reaction timing
- **Pins**:
  - GPIO14: Button (active HIGH)

---

## Project Structure

```
reaction-game-test/
├── platformio.ini           # PlatformIO config (3 environments)
├── include/                 # Shared header files
│   ├── Protocol.h           # ESP-NOW packet format
│   ├── GameTypes.h          # Game constants and enums
│   ├── AudioManager.h       # Audio playback (Host only)
│   ├── AudioDefs.h          # Sound file paths (Host only)
│   ├── display.h            # LVGL display driver (Display only)
│   ├── lgfx_conf.h          # LovyanGFX config (Display only)
│   ├── ui.h                 # SquareLine Studio UI (Display only)
│   └── ui_helpers.h         # LVGL helpers (Display only)
├── src/
│   ├── display_test.cpp     # Display program
│   ├── host_test.cpp        # Host program
│   └── joystick_test.cpp    # Joystick program
├── lib/                     # SquareLine Studio UI files
│   └── ui/
│       ├── ui.c
│       ├── ui_MainScreen.c
│       ├── display.cpp
│       ├── lgfx_conf.cpp
│       └── ui_helpers.c
└── data/                    # SPIFFS files
    ├── display/             # Images for display (~100KB total)
    │   ├── player1.png
    │   ├── player2.png
    │   ├── player3.png
    │   ├── player4.png
    │   ├── START.png
    │   ├── GO.png
    │   └── Ellipse_1.png (center circle)
    └── host/                # Audio files for host
        ├── three.mp3
        ├── two.mp3
        ├── one.mp3
        └── beep.mp3
```

---

## Setup Instructions

### 1. Copy Header Files
Copy these from `/mnt/project/` to `include/`:
- `Protocol.h`
- `GameTypes.h`
- `AudioDefs.h`
- `AudioManager.h`
- `display.h`
- `lgfx_conf.h`
- `ui.h`
- `ui_helpers.h`

### 2. Copy UI Library Files
Copy these from `/mnt/project/` to `lib/ui/`:
- `ui.c`
- ~~`ui_MainScreen.c`~~ **Use the corrected `ui_MainScreen.c` provided (has player4 uncommented + SPIFFS paths)**
- `display.cpp`
- `lgfx_conf.cpp`
- `ui_helpers.c`

**Important**: The corrected `ui_MainScreen.c` file has:
- All 4 players uncommented (previously player4 was disabled)
- All 4 playerReady circles uncommented
- SPIFFS paths instead of compiled bitmap references (`"S:/player1.png"` format)

### 3. Prepare SPIFFS Files

#### Display Images (data/display/)
1. Copy these PNG files to `data/display/`:
   - `player1.png` - "PLAYER 1" red circle
   - `player2.png` - "PLAYER 2" red circle  
   - `player3.png` - "PLAYER 3" red circle
   - `player4.png` - "PLAYER 4" red circle
   - `Pleayer_Ready_Circle.png` - Green circle overlay (shown when player ready)
   - `Big_GO_Circle.png` - Large green circle (background for GO/START)
   - `START.png` - "START" text
   - `GO.png` - "GO!" text
   - `Winner.png` - Winner screen graphic (optional for test)
2. Upload to Display ESP32-S3:
   ```bash
   pio run -e display_test -t uploadfs
   ```

#### Host Audio (data/host/)
1. Create 4 MP3 files (mono, 44.1kHz recommended):
   - `three.mp3` - "Three"
   - `two.mp3` - "Two"
   - `one.mp3` - "One"
   - `beep.mp3` - GO beep sound
2. Upload to Host ESP32:
   ```bash
   pio run -e host_test -t uploadfs
   ```

### 4. Configure Joystick IDs

**For Joystick 1** (MAC: BC:FF:4D:F9:F3:91):
```cpp
// In joystick_test.cpp
#define MY_ID             ID_STICK1
```

**For Joystick 2** (MAC: BC:FF:4D:F9:AE:29):
```cpp
// In joystick_test.cpp
#define MY_ID             ID_STICK2
```

---

## Building & Uploading

### Option 1: Build All
```bash
pio run
```

### Option 2: Build Individual Devices
```bash
pio run -e display_test
pio run -e host_test
pio run -e joystick_test
```

### Upload
```bash
pio run -e display_test -t upload    # Display
pio run -e host_test -t upload       # Host
pio run -e joystick_test -t upload   # Joystick
```

---

## Testing Procedure

### 1. Power On Sequence
1. **Display** - Should show "START" screen with player circles
2. **Host** - Should print "Host ready!" with paired devices
3. **Joystick 1** - Should print "Joystick ready!" and pair with host
4. **Joystick 2** - Same as above

### 2. Expected Game Flow

**Phase 1: IDLE (3 seconds)**
- Host: Rainbow animation on NeoPixels
- Display: "START" text shown
- Audio: "Get ready" voice

**Phase 2: COUNTDOWN (3 seconds)**
- Host: Red blinking NeoPixels
- Display: Shows countdown numbers (3, 2, 1)
- Audio: Voice counts "Three", "Two", "One"

**Phase 3: GO SIGNAL**
- Host: Green solid NeoPixels
- Display: "GO!" text appears
- Audio: Beep sound
- Joysticks: Start timing

**Phase 4: REACTION (wait for button presses)**
- Joysticks: Detect button press, send reaction time
- Host: Updates NeoPixel rings (Green=valid, Red=penalty)
- Display: Shows reaction times

**Phase 5: RESULTS (5 seconds)**
- Serial: Prints winner
- Audio: Victory fanfare
- Display: Shows winner with times
- NeoPixels: Winner's rings green, loser's red

**Loop**: Returns to IDLE after 5 seconds

### 3. Serial Monitor Output

**Host (115200 baud)**:
```
=== HOST TEST (ESP32) ===
Audio system ready
Display paired
Joystick 1 paired
Joystick 2 paired
Host MAC: 88:57:21:B3:05:AC
Host ready!

IDLE - Press button to start
Countdown: 3
Countdown: 2
Countdown: 1
GO!
Player 1: 234 ms
Player 2: 456 ms

=== RESULTS ===
Player 1: 234 ms
Player 2: 456 ms
Player 1 WINS!
```

**Joystick (115200 baud)**:
```
=== JOYSTICK TEST (ESP8266) ===
Host paired
Joystick ID: 0x01
My MAC: BC:FF:4D:F9:F3:91
Joystick ready!

Countdown: 3
Countdown: 2
Countdown: 1
GO! Waiting for button press...
Button pressed! Time: 234 ms
Sent CMD=0x26, DATA=234
```

---

## Troubleshooting

### ESP-NOW Issues
- **"Host pair failed"**: Check MAC addresses in code match actual device MACs
- **No communication**: Verify all devices on same WiFi channel (6)
- **Packet loss**: Reduce distance between devices (<10m for testing)

### Display Issues
- **Black screen**: Check LVGL buffer allocation (requires PSRAM)
- **Garbled graphics**: Verify RGB pin mapping in `lgfx_conf.cpp`
- **No UI elements**: Ensure SPIFFS images uploaded correctly

### Audio Issues
- **No sound**: Check I2S wiring to MAX98357A
- **Crackling**: Ensure MP3 files are 44.1kHz mono
- **File not found**: Verify SPIFFS upload succeeded

### NeoPixel Issues
- **No lights**: Check GPIO4 connection and 5V power
- **Wrong colors**: Adjust NEO_GRB vs NEO_RGB in code
- **Flickering**: Add 1000µF capacitor on 5V rail

---

## Next Steps

Once this test works:
1. Add MPU-6050 shake detection to joysticks
2. Implement full game state machine (5 rounds, scoring)
3. Add remaining audio files (20+ total)
4. Create full UI screens (Join, Mode Select, Results, Winner)
5. Add touch input handling on display
6. Implement all display commands from Protocol.h

---

## File Sizes (Approximate)

- **Display firmware**: ~1.2 MB (LVGL + UI)
- **Host firmware**: ~1.0 MB (Audio + ESP-NOW)
- **Joystick firmware**: ~300 KB (minimal)
- **Display SPIFFS**: ~100 KB (images)
- **Host SPIFFS**: ~500 KB (audio files)

---

## MAC Address Reference

| Device | MAC Address |
|--------|-------------|
| Host ESP32 | 88:57:21:B3:05:AC |
| Display ESP32-S3 | D0:CF:13:01:D1:A4 |
| Joystick 1 ESP8266 | BC:FF:4D:F9:F3:91 |
| Joystick 2 ESP8266 | BC:FF:4D:F9:AE:29 |

---

## License
Educational project - Reaction Reimagined Game
