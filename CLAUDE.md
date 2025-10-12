# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based omni-directional robot controller with 4 DC motors in X-configuration, using TA6586 H-bridge motor drivers. Features a web interface for control and motor calibration.

## Build & Upload Commands

```bash
# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor

# Build and upload in one command
pio run --target upload && pio device monitor
```

## Hardware Configuration

### Motor Drivers (TA6586)
- **Driver 1**: Motors 1 & 2 (GPIO pins 32,33,25,26)
- **Driver 2**: Motors 3 & 4 (GPIO pins 19,18,17,16)

### TA6586 Control Logic (CRITICAL)
The TA6586 has asymmetric control that must be followed exactly:

**Forward direction:**
- D0 = HIGH/PWM (normal PWM, higher value = faster)
- D1 = LOW

**Backward direction:**
- D0 = LOW/PWM (INVERTED PWM: `255 - speed`)
- D1 = HIGH

This means for backward motion, the PWM value must be inverted. This is implemented in `setPhysicalMotor()` at src/main.cpp:164-179.

### X-Configuration Kinematics
Motors arranged in X-pattern (viewed from top):
```
    M1 ↗  ↖ M2
        ╲╱
        ╱╲
    M3 ↙  ↘ M4
```

Movement logic (src/main.cpp:212-252):
- **Forward**: All motors +speed
- **Backward**: All motors -speed
- **Strafe Left**: M1,M4 negative; M2,M3 positive
- **Strafe Right**: M1,M4 positive; M2,M3 negative
- **Rotate Left**: M2,M4 positive; M1,M3 negative
- **Rotate Right**: M1,M3 positive; M2,M4 negative

## Architecture

### Motor Control Abstraction
Three-layer motor control system:
1. **Physical Motors** (`setPhysicalMotor`): Controls actual hardware with TA6586-specific logic
2. **Logical Motors** (`setMotor`): Applies user-configured mapping and inversion
3. **Movement Functions** (`moveForward`, `rotateLeft`, etc.): High-level kinematics

### Configuration System
- **Motor Mapping**: Maps logical positions (0-3) to physical motors (1-4)
- **Motor Inversion**: Per-motor direction reversal
- **Storage**: Persisted to ESP32 NVS using Preferences library
- **Default**: Direct mapping (logical N → physical N+1)

Configuration keys in EEPROM: `map0-map3` (int), `inv0-inv3` (bool)

### Web Interface
Single-page application with two tabs embedded in PROGMEM (src/main.cpp:398-1115):

**Control Tab**:
- Two-column layout (directions on left, rotation on right)
- Speed slider (0-255 PWM)
- Emergency stop button

**Calibration Tab**:
- 2×2 visual grid matching physical robot layout
- Per-motor testing (forward/backward buttons)
- Interactive motor mapping and direction inversion
- Save to EEPROM functionality

### WebSocket Protocol
Text-based commands over WebSocket (`/ws`):

**Movement**: `forward`, `backward`, `left`, `right`, `rotate_left`, `rotate_right`, `stop`

**Calibration**: `test_{0-3}_{fwd|bwd|stop}` (tests logical motor at position)

**Configuration**:
- `get_config` → returns JSON `{mapping:[...], invert:[...]}`
- `set_map:{pos}:{motor}` → map logical position to physical motor
- `set_inv:{pos}:{true|false}` → set direction inversion
- `save_config` → persist to EEPROM
- `reset_config` → restore defaults

**Speed**: `speed:{0-255}` → set current speed

## WiFi Configuration

Default credentials (src/main.cpp:10-11):
- SSID: `DiasPhone`
- Password: `diasdias`

After successful connection, web interface available at ESP32's IP address (printed to serial).

## PWM Settings

- Frequency: 5000 Hz
- Resolution: 8-bit (0-255)
- 4 PWM channels mapped to motor D0 pins
- D1 pins are GPIO (direction control)

## Important Implementation Notes

1. **Always use `abs(speed)` for forward direction** but **`255 - abs(speed)` for backward** due to TA6586 inverted PWM requirement

2. **10μs delay between D1 and PWM changes** to ensure driver registers direction before speed

3. **Motor mapping affects calibration commands**: `test_N` commands operate on logical positions, which are remapped to physical motors

4. **WebSocket disconnection triggers motor stop** for safety (src/main.cpp:384-385)

5. **HTML is in PROGMEM**: The entire web interface is compiled into flash memory as a raw string literal

## Common Modifications

### Changing WiFi credentials
Edit lines 10-11 in src/main.cpp

### Adjusting motor pin assignments
Modify `#define MOTOR{1-4}_D{0,1}` at src/main.cpp:15-24

### Changing PWM frequency
Modify `PWM_FREQ` at src/main.cpp:27 (affects motor smoothness and noise)

### Modifying web interface
Edit the `index_html[]` PROGMEM string starting at src/main.cpp:398

---

## Branch: web-bluetooth-control

**Alternative implementation using Web Bluetooth API instead of WiFi.**

### Overview

This branch implements browser-based control using the Web Bluetooth API, providing a Progressive Web App (PWA) hosted on GitHub Pages. No WiFi credentials or app installation required - users control the robot directly from Chrome/Edge browser.

### Architecture Differences from Main Branch

**Communication**: BLE GATT instead of WiFi WebSocket
**Web Interface**: Separate PWA on GitHub Pages (docs/ folder) instead of PROGMEM embedded HTML
**Deployment**: Automatic via GitHub Actions to GitHub Pages
**Power**: Lower power consumption (BLE vs WiFi)
**Platform Support**: Android/Windows/macOS/Linux Chrome only (no iOS Safari)

### BLE Service Architecture

**Service UUID**: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
**Device Name**: `Omni Robot`

**Characteristics**:

1. **Command Characteristic** (`beb5483e-36e1-4688-b7f5-ea07361b26a8`)
   - Type: Write
   - Format: UTF-8 string
   - Commands: `forward`, `backward`, `strafe_left`, `strafe_right`, `rotate_left`, `rotate_right`, `stop`

2. **Joystick Characteristic** (`ca73b3ba-39f6-4ab3-91ae-186dc9577d99`)
   - Type: Write
   - Format: 2 bytes (int8_t x, int8_t y)
   - Range: -128 to 127 (mapped from -255 to 255)

3. **Speed Characteristic** (`1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e`)
   - Type: Write
   - Format: 1 byte (uint8_t)
   - Range: 0-255 PWM value

4. **Config Characteristic** (`d4e1f1a2-8b5c-4d3e-9f7a-6c8b5a4d3e2f`)
   - Type: Read/Write/Notify
   - Format: JSON string `{"mapping":[1,2,3,4],"invert":[false,false,false,false]}`
   - Special commands: `save`, `reset`, `set_map:{pos}:{motor}`, `set_inv:{pos}:{0|1}`

5. **Test Motor Characteristic** (`a3b2c1d4-5e6f-7a8b-9c0d-1e2f3a4b5c6d`)
   - Type: Write
   - Format: UTF-8 string
   - Commands: `test_{0-3}_{fwd|bwd|stop}` for motor calibration

### Web Interface Structure

```
docs/
├── index.html              # Main PWA interface
├── css/style.css          # Complete styling
├── js/
│   ├── bluetooth.js       # Web Bluetooth API wrapper (RobotBluetooth class)
│   ├── joystick.js        # Canvas-based joystick (Joystick class)
│   └── app.js             # Main application logic
├── manifest.json          # PWA manifest
├── service-worker.js      # Offline support
└── icons/                 # PWA icons (192x192, 512x512)
```

### Key Implementation Details (BLE Branch)

**ESP32 BLE Server** (src/main.cpp):
- Uses ESP32 Arduino Core BLE library (built-in, no external deps)
- Advertises as "Omni Robot"
- Handles client connection/disconnection with automatic motor stop
- Same motor control logic as other branches (TA6586 asymmetric PWM)
- Joystick input mapped from int8_t (-128..127) to motor speed (-255..255)

**Web Bluetooth API** (docs/js/bluetooth.js):
- `RobotBluetooth` class wraps all BLE communication
- Filters for device name "Omni Robot" during pairing
- Discovers service and all 5 characteristics on connect
- Listens for config notifications to sync calibration state
- Handles disconnection events and reconnection

**Canvas Joystick** (docs/js/joystick.js):
- `Joystick` class renders interactive control on canvas
- Supports both touch and mouse events
- Calculates polar coordinates and clamps to circular boundary
- Outputs -255..255 range for x/y axes
- Auto-returns to center with visual feedback

**Progressive Web App**:
- Installable to home screen via manifest.json
- Offline support via service-worker.js caching strategy
- Requires HTTPS (provided by GitHub Pages)
- Works offline after first load

### GitHub Actions Deployment

**.github/workflows/deploy.yml**:
- Triggers on push to `web-bluetooth-control` branch when `docs/` changes
- Also supports manual trigger via `workflow_dispatch`
- Deploys `docs/` folder to GitHub Pages
- Requires Pages enabled in repo settings (Settings → Pages → Source: GitHub Actions)

**Deployment URL**: `https://{username}.github.io/{repo}/`

### Browser Compatibility

**Supported**:
- Chrome 56+ (Android, Windows, macOS, Linux)
- Edge 79+ (Windows, macOS)

**Not Supported**:
- iOS Safari (no Web Bluetooth API)
- Firefox (Web Bluetooth disabled by default)

**iOS Workaround**: Use [Bluefy Browser](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055) from App Store

### Development Workflow

**ESP32 Development**:
```bash
git checkout web-bluetooth-control
pio run --target upload
pio device monitor
```

**Web Interface Development**:
- Edit files in `docs/` folder
- Test locally with HTTPS server (required for Web Bluetooth):
  ```bash
  cd docs
  python -m http.server 8000
  # Then use ngrok for HTTPS: ngrok http 8000
  ```
- Or push to GitHub and test on Pages URL

**Updating Service Worker**:
- Increment `CACHE_NAME` version in service-worker.js when changing cached files
- Old caches automatically deleted on activation

### Troubleshooting (BLE Branch)

**"Bluetooth is not available"**:
- Must use Chrome/Edge (not Firefox/Safari)
- Must be HTTPS (GitHub Pages provides this)
- Check: https://caniuse.com/web-bluetooth

**ESP32 not in device list**:
- Verify firmware uploaded and ESP32 powered
- Check serial monitor shows "BLE сервер запущен"
- BLE name must be exactly "Omni Robot"
- Try restarting ESP32

**Connection drops frequently**:
- Check power supply stability
- Reduce distance between device and ESP32
- Avoid WiFi interference (BLE uses 2.4GHz)

**Config changes not saving**:
- Click "💾 Сохранить настройки" button after calibration
- Check serial monitor for EEPROM write confirmation
- Config persists across power cycles
