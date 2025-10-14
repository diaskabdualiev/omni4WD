# ESP32 Omni Robot Controller - Web Bluetooth Edition 🌐🤖

Progressive Web App для управления 4-колесным omni-роботом через Bluetooth Low Energy прямо из браузера!

## ✨ Features

- **🌐 Web Bluetooth API**: Управление прямо из браузера без установки приложений
- **📱 Progressive Web App**: Установи на домашний экран, работает офлайн
- **🎮 Два режима управления**: Джойстик (canvas) и кнопочный
- **⚙️ Калибровка моторов**: Визуальный 2x2 grid для настройки
- **💾 Persistent Settings**: Конфигурация сохраняется в ESP32 EEPROM
- **🔋 Низкое энергопотребление**: BLE экономичнее WiFi

## 🌍 Browser Compatibility

| Platform | Browser | Status |
|----------|---------|--------|
| Android | Chrome 56+ | ✅ Полная поддержка |
| Windows | Chrome 56+ | ✅ Полная поддержка |
| macOS | Chrome 56+ | ✅ Полная поддержка |
| Linux | Chrome 56+ | ✅ Полная поддержка |
| iOS/iPadOS | Safari | ❌ Не поддерживается |
| iOS | Bluefy Browser | ✅ Альтернатива (App Store) |

**Важно**: iOS Safari не поддерживает Web Bluetooth API. Для iOS используйте [Bluefy Browser](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055) из App Store.

## 🚀 Quick Start

### 1. Загрузить firmware на ESP32

```bash
# Переключиться на ветку web-bluetooth-control
git checkout web-bluetooth-control

# Скомпилировать и загрузить
pio run --target upload

# Открыть монитор порта
pio device monitor
```

### 2. Открыть веб-интерфейс

Откройте в Chrome (или другом поддерживаемом браузере):

**🌐 https://diaskabdualiev.github.io/omni4WD/**

### 3. Подключиться к роботу

1. Нажмите **"🔗 Подключить робота"**
2. Выберите **"Omni Robot"** из списка устройств
3. Разрешите доступ к Bluetooth
4. Управляйте роботом! 🎉

## 🛠️ Hardware Requirements

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

## 💻 Software Requirements

- [PlatformIO](https://platformio.org/)
- Modern web browser with Web Bluetooth support (Chrome recommended)
- ESP32 Arduino Core (auto-installed)

## 📖 Usage

### Control Tab

**Joystick Mode (Default):**
- Drag the joystick to control movement and rotation
- Left/Right = robot rotation
- Up/Down = forward/backward
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
4. Click **"💾 Сохранить настройки"** to save to EEPROM

## 🔧 Technical Details

### BLE Service Architecture

```
Service UUID: 4fafc201-1fb5-459e-8fcc-c5c9c331914b

├── Command Characteristic (movement commands)
├── Joystick Characteristic (x, y coordinates)
├── Speed Characteristic (0-255)
├── Config Characteristic (motor mapping & inversion)
└── Test Motor Characteristic (calibration)
```

### TA6586 Motor Control

The TA6586 has asymmetric control requiring special handling:

**Forward:**
- D0 = HIGH/PWM (normal PWM: higher = faster)
- D1 = LOW

**Backward:**
- D0 = LOW/PWM (inverted PWM: `255 - speed`)
- D1 = HIGH

Implemented in `setPhysicalMotor()` at `src/main.cpp:124-171`.

### PWM Settings

- Frequency: 5000 Hz
- Resolution: 8-bit (0-255)
- Default speed: 200 (~80%)

### Movement Algorithm

X-configuration kinematics:

- **Forward**: All motors +speed
- **Backward**: All motors -speed
- **Strafe Left**: M1,M4 negative; M2,M3 positive
- **Strafe Right**: M1,M4 positive; M2,M3 negative
- **Rotate Left**: M2,M4 positive; M1,M3 negative
- **Rotate Right**: M1,M3 positive; M2,M4 negative

## 🏗️ Development

### Project Structure

```
web-bluetooth-control/
├── src/
│   └── main.cpp              # ESP32 BLE firmware
├── docs/                     # GitHub Pages (web interface)
│   ├── index.html
│   ├── css/style.css
│   ├── js/
│   │   ├── app.js            # Main app logic
│   │   ├── bluetooth.js      # Web Bluetooth API
│   │   └── joystick.js       # Canvas joystick
│   ├── manifest.json         # PWA manifest
│   └── service-worker.js     # Offline support
├── .github/workflows/
│   └── deploy.yml            # Auto-deploy to Pages
└── platformio.ini
```

### Local Development

1. **ESP32 Development:**
   ```bash
   pio run                    # Compile
   pio run --target upload    # Upload to ESP32
   pio device monitor         # View serial output
   ```

2. **Web Interface Development:**
   - Open `docs/index.html` in a local web server
   - Must use HTTPS for Web Bluetooth to work
   - Use `python -m http.server 8000` + ngrok for local testing

### Deploy to GitHub Pages

1. Push to `web-bluetooth-control` branch
2. GitHub Actions will automatically deploy `docs/` folder
3. Enable Pages in repo settings: Settings → Pages → Source: GitHub Actions

## 🐛 Troubleshooting

**ESP32 not showing up in Bluetooth list:**
- Ensure ESP32 is powered and firmware is loaded
- Check serial monitor for "BLE сервер запущен"
- Try restarting ESP32

**"Bluetooth is not available" error:**
- Use Chrome or Edge (not Firefox or Safari)
- Ensure HTTPS (GitHub Pages provides this)
- Check browser supports Web Bluetooth: https://caniuse.com/web-bluetooth

**Motors run wrong direction:**
- Use Calibration tab to configure motor mapping
- Enable "Реверс" checkboxes for inverted motors
- Save settings to EEPROM

**Robot moves diagonally instead of straight:**
- Check motor calibration
- Ensure all motors are same type/speed
- Verify X-configuration wiring

## 📚 Branch Information

This is the **`web-bluetooth-control`** branch:
- **Control Method**: Web Bluetooth API (browser-based)
- **Platform**: Android, Windows, macOS, Linux (Chrome)
- **Interface**: Progressive Web App on GitHub Pages

### Other Branches:

- **`main`**: WiFi + WebSocket version (full platform support)
  ```bash
  git checkout main
  ```

- **`wiimote-control`**: Nintendo Wii Remote version (Bluetooth Classic)
  ```bash
  git checkout wiimote-control
  ```

## 🎯 Why Web Bluetooth?

**Pros:**
- ✅ No app installation required
- ✅ Cross-platform (Android, Windows, macOS, Linux)
- ✅ Easy updates (just refresh page)
- ✅ Lower power consumption than WiFi
- ✅ Works offline after first load (PWA)

**Cons:**
- ❌ No iOS Safari support
- ❌ Requires HTTPS
- ❌ Limited to Chrome/Edge browsers

## 📄 License

MIT

## 🙏 Credits

- ESP32 Arduino Core BLE library
- Web Bluetooth Community Group
- Built with PlatformIO

---

**🤖 Created for omni-directional robot platform with ESP32, TA6586 drivers, and Web Bluetooth API**

Need help? Check the [CLAUDE.md](CLAUDE.md) for development guidelines or open an issue!
