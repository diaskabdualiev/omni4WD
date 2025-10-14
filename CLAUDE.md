# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based omni-directional robot controller with 4 DC motors in X-configuration, using TA6586 H-bridge motor drivers. **Wiimote-Control Branch**: Nintendo Wii Remote control via Bluetooth Classic.

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

Movement logic (src/main.cpp:262-292):
- **Forward**: All motors +speed
- **Backward**: All motors -speed
- **Strafe Left**: M1-, M2+, M3+, M4- (D-pad UP)
- **Strafe Right**: M1+, M2-, M3-, M4+ (D-pad DOWN)
- **Rotate Left**: M1-, M2+, M3-, M4+ (Button B/1)
- **Rotate Right**: M1+, M2-, M3+, M4- (Button A/2)

## Wiimote Controls (Horizontal Orientation)

**Movement (D-Pad):**
- LEFT (←): Forward
- RIGHT (→): Backward
- UP (↑): Strafe Left
- DOWN (↓): Strafe Right

**Rotation (Buttons):**
- A / 2: Rotate Right
- B / 1: Rotate Left

**Speed Control:**
- PLUS (+): Increase speed by 25 (max 255)
- MINUS (-): Decrease speed by 25 (min 50)
- Speed changes limited to 200ms intervals to prevent rapid changes

**Safety:**
- HOME: Emergency stop (highest priority)

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

### Wiimote Input System

**Vector-Based Movement** (src/main.cpp:258-307):
- Multiple buttons can be pressed simultaneously
- Each button adds its contribution to motor speeds (float -1.0 to +1.0)
- Motor speeds are normalized to prevent overcurrent
- Final speeds scaled by `currentSpeed` (adjustable via +/- buttons)

**Button State Detection**:
- Uses 16-bit bitmask from ESP32Wiimote library
- Detects state changes for debug output
- HOME button has highest priority (immediate stop)

**Speed Change Debouncing**:
- 200ms minimum interval between speed changes
- Prevents accidental rapid changes
- Speed range: 50-255 (min-max)
- Step size: 25 per button press

## PWM Settings

- Frequency: 5000 Hz
- Resolution: 8-bit (0-255)
- 4 PWM channels mapped to motor D0 pins
- D1 pins are GPIO (direction control)

## Important Implementation Notes

1. **Always use `abs(speed)` for forward direction** but **`255 - abs(speed)` for backward** due to TA6586 inverted PWM requirement

2. **10μs delay between D1 and PWM changes** to ensure driver registers direction before speed

3. **Motor mapping affects all movement**: Logical positions are remapped to physical motors

4. **Vector normalization prevents overcurrent**: When multiple buttons pressed, motor speeds scaled proportionally

5. **Speed control debouncing**: 200ms minimum interval prevents rapid uncontrolled changes

## Common Modifications

### Adjusting motor pin assignments
Modify `#define MOTOR{1-4}_D{0,1}` at src/main.cpp:8-17

### Changing PWM frequency
Modify `PWM_FREQ` at src/main.cpp:20 (affects motor smoothness and noise)

### Adjusting speed control parameters
- `SPEED_STEP` (line 51): Amount to increase/decrease per button press (default: 25)
- `SPEED_CHANGE_INTERVAL` (line 50): Minimum ms between changes (default: 200)
- Min speed: 50 (line 252)
- Max speed: 255 (line 248)
- Default speed: 200 (line 36)

### Button remapping
Edit button conditions in `handleWiimoteInput()` at src/main.cpp:262-292
