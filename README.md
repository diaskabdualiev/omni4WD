# ESP32 Omni Robot Controller

Web-based controller for 4-wheel omni-directional robot using ESP32 and TA6586 motor drivers.

## Features

- **Web Interface**: Control robot via WiFi from any device
- **Two Control Modes**:
  - 🕹️ **Joystick Mode**: Canvas-based joystick for smooth directional control
  - 🎮 **Button Mode**: Grid buttons for precise movements
- **Motor Calibration**: Interactive 2x2 grid for motor mapping and direction configuration
- **Persistent Settings**: Motor configuration saved to ESP32 NVS (EEPROM)
- **Omni-Directional Movement**: Forward, backward, strafe left/right, and rotation

## Hardware Requirements

- ESP32 Development Board (ESP32-DEVKIT)
- 4× DC Motors with omni wheels (X-configuration)
- 2× TA6586 H-Bridge Motor Drivers
- Power supply for motors

### Pin Configuration

**Driver 1:**
- Motor 1: GPIO 32 (PWM), GPIO 33 (Direction)
- Motor 2: GPIO 25 (PWM), GPIO 26 (Direction)

**Driver 2:**
- Motor 3: GPIO 19 (PWM), GPIO 18 (Direction)
- Motor 4: GPIO 17 (PWM), GPIO 16 (Direction)

### Motor Layout (X-Configuration)

```
    M1 ↗  ↖ M2
        ╲╱
        ╱╲
    M3 ↙  ↘ M4
```

## Software Requirements

- [PlatformIO](https://platformio.org/)
- Libraries (auto-installed):
  - ESPAsyncWebServer
  - AsyncTCP
  - ESP32 Arduino Core

## Installation

1. Clone this repository
2. Update WiFi credentials in `src/main.cpp`:
   ```cpp
   const char* ssid = "YourWiFiSSID";
   const char* password = "YourPassword";
   ```
3. Build and upload:
   ```bash
   pio run --target upload
   pio device monitor
   ```
4. Connect to the web interface at the IP address shown in serial monitor

## Usage

### Control Tab

**Joystick Mode (Default):**
- Drag the joystick circle to control movement and rotation
- Left/Right movement on joystick = rotation
- Up/Down movement = forward/backward
- Use ⟲⟳ buttons for strafing left/right

**Button Mode:**
- ⬆️ Forward | ⬇️ Backward
- ⬅️ Rotate Left | ➡️ Rotate Right
- ⟲ Strafe Left | ⟳ Strafe Right
- ⏹️ Emergency Stop

### Calibration Tab

1. Test each motor corner using ⬆️⬇️ buttons
2. Select the correct physical motor from dropdown
3. Enable "Реверс" (Reverse) if motor spins wrong direction
4. Click "💾 Сохранить настройки" to save to EEPROM

## Technical Details

### TA6586 Motor Control

The TA6586 has asymmetric control requiring special handling:

**Forward:**
- D0 = HIGH/PWM (normal PWM: higher = faster)
- D1 = LOW

**Backward:**
- D0 = LOW/PWM (inverted PWM: `255 - speed`)
- D1 = HIGH

This is implemented in `setPhysicalMotor()` function.

### PWM Settings

- Frequency: 5000 Hz
- Resolution: 8-bit (0-255)
- Default speed: 200 (~80%)

### WebSocket Commands

- `forward`, `backward`, `left`, `right`, `rotate_left`, `rotate_right`, `stop`
- `joy:X:Y` - Joystick control (X, Y: -255 to 255)
- `speed:N` - Set speed (0-255)
- `test_N_{fwd|bwd|stop}` - Test motor at position N (0-3)
- `set_map:POS:MOTOR` - Map logical position to physical motor
- `set_inv:POS:{true|false}` - Set motor direction inversion
- `save_config` - Save configuration to EEPROM

## License

MIT

## Author

Created for omni-directional robot platform with ESP32 and TA6586 drivers.
