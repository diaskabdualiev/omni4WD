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

**Button-based Movement Commands** (src/main.cpp:222-262):
- **Forward**: All motors +speed
- **Backward**: All motors -speed
- **Strafe Left**: M1,M4 negative; M2,M3 positive
- **Strafe Right**: M1,M4 positive; M2,M3 negative
- **Rotate Left**: M2,M4 positive; M1,M3 negative
- **Rotate Right**: M1,M3 positive; M2,M4 negative

**Joystick Vector Control** (src/main.cpp:357-390):
The robot supports two drive modes for joystick control:

1. **Omni Mode (Strafe)** - `omniMode = true` (default):
   - X-axis: Strafe left/right (sideways movement)
   - Y-axis: Forward/backward
   - Formulas: `M1=Y+X, M2=Y-X, M3=Y+X, M4=Y-X`
   - When joystick full right: Robot strafes right
   - Ideal for precise positioning with omni wheels

2. **Tank Mode (Rotation)** - `omniMode = false`:
   - X-axis: Rotate left/right (turn in place)
   - Y-axis: Forward/backward
   - Formulas: `M1=Y-X, M2=Y+X, M3=Y-X, M4=Y+X`
   - When joystick full right: Robot rotates clockwise
   - Behaves like traditional tank/differential drive

Mode can be switched via commands `mode_omni` / `mode_tank` and is saved to EEPROM.

## Architecture

### Motor Control Abstraction
Three-layer motor control system:
1. **Physical Motors** (`setPhysicalMotor`): Controls actual hardware with TA6586-specific logic
2. **Logical Motors** (`setMotor`): Applies user-configured mapping and inversion
3. **Movement Functions** (`moveForward`, `rotateLeft`, etc.): High-level kinematics

### Configuration System
- **Motor Mapping**: Maps logical positions (0-3) to physical motors (1-4)
- **Motor Inversion**: Per-motor direction reversal
- **Drive Mode**: Omni (strafe) vs Tank (rotation) for joystick control
- **Storage**: Persisted to ESP32 NVS using Preferences library
- **Default**: Direct mapping (logical N → physical N+1), Omni mode enabled

Configuration keys in EEPROM: `map0-map3` (int), `inv0-inv3` (bool), `omniMode` (bool)

### Web Interface
Single-page application with two tabs embedded in PROGMEM (src/main.cpp:449-1367):

**Control Tab**:
- Control mode switcher: Joystick vs Buttons
- Drive mode switcher: Omni (strafe) vs Tank (rotation)
- Joystick mode: Canvas-based virtual joystick with vector control
- Button mode: Directional control buttons
- Speed slider (0-255 PWM)
- Emergency stop button
- Dynamic UI hints showing current drive mode behavior

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
- `get_config` → returns JSON `{mapping:[...], invert:[...], omniMode:true|false}`
- `set_map:{pos}:{motor}` → map logical position to physical motor
- `set_inv:{pos}:{true|false}` → set direction inversion
- `save_config` → persist to EEPROM
- `reset_config` → restore defaults

**Drive Mode**:
- `mode_omni` → switch to Omni mode (strafe)
- `mode_tank` → switch to Tank mode (rotation)

**Speed**: `speed:{0-255}` → set current speed

**Joystick**: `joy:{x}:{y}` → vector control where x,y are -255 to 255

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
