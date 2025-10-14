# ESP32 Omni Robot Controller - Wii Remote Edition 🎮

Nintendo Wii Remote controlled 4-wheel omni-directional robot using ESP32 and TA6586 motor drivers.

## Features

- **🎮 Nintendo Wii Remote Control**: Wireless control via Bluetooth
- **Multi-Button Support**: Press multiple buttons simultaneously for diagonal/combined movements
- **Omni-Directional Movement**: Forward, backward, strafe, rotation, and diagonal movements
- **Vector-Based Control**: Smooth movement combining multiple inputs
- **Persistent Motor Calibration**: Motor configuration loaded from ESP32 NVS (EEPROM)
- **Emergency Stop**: Instant safety stop with HOME button

## Hardware Requirements

- ESP32 Development Board (ESP32-DEVKIT)
- Nintendo Wii Remote (Wiimote)
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
  - [ESP32Wiimote](https://github.com/bigw00d/ESP32Wiimote)
  - ESP32 Arduino Core
  - Preferences (EEPROM)

## Installation

1. Clone this repository
2. Build and upload:
   ```bash
   pio run --target upload
   pio device monitor
   ```
3. Pair Wii Remote:
   - Press **1 + 2** buttons simultaneously on Wiimote
   - Wait for connection (LEDs will light up)
   - Start controlling!

## Controls

### Wiimote Button Mapping (Horizontal Orientation)

Hold your Wiimote **horizontally** like a TV remote:

**D-Pad Movement:**
- **← (Left)** → Forward 🚗
- **→ (Right)** → Backward 🔙
- **↑ (Up)** → Strafe Left ⬅️
- **↓ (Down)** → Strafe Right ➡️

**Rotation (Turn in Place):**
- **A Button** → Rotate Right ↷
- **B Button** → Rotate Left ↶
- **2 Button** → Rotate Right (same as A)
- **1 Button** → Rotate Left (same as B)

**Speed Control:**
- **+ Button** → Increase Speed ⚡ (+25 per press, max 255)
- **- Button** → Decrease Speed 🐌 (-25 per press, min 50)

**Emergency:**
- **HOME Button** → 🛑 **EMERGENCY STOP**

### Multi-Button Combinations

You can press **multiple buttons at once** for advanced movements:

- **← + ↑** = Diagonal forward-left ↖️
- **← + ↓** = Diagonal forward-right ↗️
- **→ + ↑** = Diagonal backward-left ↙️
- **→ + ↓** = Diagonal backward-right ↘️
- **← + A** = Forward while rotating right 🔄
- **Any combination!** = Vector sum of all inputs

The system automatically normalizes motor speeds to prevent overcurrent.

## Technical Details

### TA6586 Motor Control

The TA6586 has asymmetric control requiring special handling:

**Forward:**
- D0 = HIGH/PWM (normal PWM: higher = faster)
- D1 = LOW

**Backward:**
- D0 = LOW/PWM (inverted PWM: `255 - speed`)
- D1 = HIGH

This is implemented in `setPhysicalMotor()` function at `src/main.cpp:82-129`.

### PWM Settings

- Frequency: 5000 Hz
- Resolution: 8-bit (0-255)
- Default speed: 200 (~80%)

### Movement Algorithm

The robot uses **vector-based movement** (see `src/main.cpp:239-307`):

1. Each button adds its contribution to motor speeds
2. Forward/Backward (D-pad ←/→): All motors +1 or -1
3. Strafe (D-pad ↑/↓): M1∓, M2±, M3±, M4∓ (sideways movement)
4. Rotation (A/B buttons): M1±, M2∓, M3±, M4∓ (turn in place)
5. Sum all inputs → Normalize to max speed → Apply to motors

### Motor Calibration

Motor mapping and direction inversion are loaded from EEPROM on startup. To recalibrate:
1. Use the WiFi version on `main` branch for calibration UI
2. Save configuration to EEPROM
3. Switch back to `wiimote-control` branch
4. Settings are preserved in EEPROM

## Architecture

```
ESP32Wiimote Library
       ↓
Button State Detection (uint16_t bitmask)
       ↓
Vector Addition (float motor1-4)
       ↓
Normalization (scale to currentSpeed)
       ↓
Motor Mapping (logical → physical)
       ↓
Inversion (if configured)
       ↓
TA6586 Driver (asymmetric PWM)
       ↓
DC Motors (X-configuration)
```

## Troubleshooting

**Wiimote won't connect:**
- Ensure batteries are fresh
- Press 1+2 within range (< 10m)
- ESP32 Bluetooth must be enabled
- Check Serial Monitor for connection status

**Motors run wrong direction:**
- Calibrate using WiFi version (`main` branch)
- Or manually edit EEPROM in `loadConfig()` function

**Robot moves diagonally instead of straight:**
- Check motor calibration
- Ensure all motors are same type/speed
- Verify X-configuration wiring

## Serial Monitor Output

```
=================================
   ESP32 Omni Robot Controller
   Nintendo Wii Remote Edition
=================================

Конфигурация загружена из EEPROM:
  Маппинг: [1, 2, 3, 4]
  Инверсия: [0, 0, 0, 0]

✓ Моторы инициализированы
✓ Wiimote инициализирован

=================================
Нажми 1+2 на Wiimote для подключения
=================================

Управление (Wiimote в ГОРИЗОНТАЛЬНОМ положении):
  D-pad ←   = Вперёд
  D-pad →   = Назад
  ...
```

## Branch Information

This is the **`wiimote-control`** branch:
- **Control Method**: Nintendo Wii Remote via Bluetooth
- **No WiFi required**
- **No web interface**

For the **WiFi web interface version**, see the `main` branch:
```bash
git checkout main
```

## License

MIT

## Credits

- ESP32Wiimote library by [bigw00d](https://github.com/bigw00d/ESP32Wiimote)
- Built with PlatformIO and ESP32 Arduino Core

---

**Created for omni-directional robot platform with ESP32, TA6586 drivers, and Nintendo Wii Remote** 🤖🎮
