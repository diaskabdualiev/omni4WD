#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>

// ==================== КОНФИГУРАЦИЯ ====================

// WiFi настройки
const char* ssid = "DiasPhone";
const char* password = "diasdias";

// Пины моторов (TA6586 драйверы)
// Драйвер 1
#define MOTOR1_D0 32  // PWM для вперед
#define MOTOR1_D1 33  // Направление (LOW/HIGH)
#define MOTOR2_D0 25  // PWM для вперед
#define MOTOR2_D1 26  // Направление (LOW/HIGH)

// Драйвер 2
#define MOTOR3_D0 19  // PWM для вперед
#define MOTOR3_D1 18  // Направление (LOW/HIGH)
#define MOTOR4_D0 17  // PWM для вперед
#define MOTOR4_D1 16  // Направление (LOW/HIGH)

// PWM настройки
#define PWM_FREQ 5000      // 5 кГц
#define PWM_RESOLUTION 8   // 8 бит (0-255)

// PWM каналы для каждого мотора
#define PWM_CHANNEL_M1 0
#define PWM_CHANNEL_M2 1
#define PWM_CHANNEL_M3 2
#define PWM_CHANNEL_M4 3

// ==================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ====================

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Preferences preferences;

// Текущая скорость (0-255)
int currentSpeed = 200;  // ~80% от 255

// Режим управления: true = Omni (strafe), false = Tank (rotation)
bool omniMode = true;

// Конфигурация моторов
// motorMapping[логическая_позиция] = физический_мотор
// Логические позиции: 0=передний-правый, 1=передний-левый, 2=задний-левый, 3=задний-правый
// По умолчанию для X-конфигурации: M1↗ M2↖ M3↙ M4↘
int motorMapping[4] = {1, 2, 3, 4};  // По умолчанию: прямое соответствие
bool motorInvert[4] = {false, false, false, false};  // Инверсия направления

// ==================== ФУНКЦИИ РАБОТЫ С НАСТРОЙКАМИ ====================

void loadConfig() {
  preferences.begin("robot", true);  // true = read-only

  // Загрузка маппинга моторов
  for (int i = 0; i < 4; i++) {
    String key = "map" + String(i);
    motorMapping[i] = preferences.getInt(key.c_str(), i + 1);  // По умолчанию 1,2,3,4

    key = "inv" + String(i);
    motorInvert[i] = preferences.getBool(key.c_str(), false);  // По умолчанию не инвертировано
  }

  omniMode = preferences.getBool("omniMode", true);

  preferences.end();

  Serial.println("\nКонфигурация загружена из EEPROM:");
  Serial.print("  Маппинг: [");
  for (int i = 0; i < 4; i++) {
    Serial.print(motorMapping[i]);
    if (i < 3) Serial.print(", ");
  }
  Serial.println("]");
  Serial.print("  Инверсия: [");
  for (int i = 0; i < 4; i++) {
    Serial.print(motorInvert[i] ? "1" : "0");
    if (i < 3) Serial.print(", ");
  }
  Serial.println("]");
  Serial.printf("  Режим: %s\n", omniMode ? "Omni (strafe)" : "Tank (rotation)");
}

void saveConfig() {
  preferences.begin("robot", false);  // false = read-write

  for (int i = 0; i < 4; i++) {
    String key = "map" + String(i);
    preferences.putInt(key.c_str(), motorMapping[i]);

    key = "inv" + String(i);
    preferences.putBool(key.c_str(), motorInvert[i]);
  }

  preferences.putBool("omniMode", omniMode);

  preferences.end();
  Serial.println("✓ Конфигурация сохранена в EEPROM");
}

void resetConfig() {
  motorMapping[0] = 1;
  motorMapping[1] = 2;
  motorMapping[2] = 3;
  motorMapping[3] = 4;

  motorInvert[0] = false;
  motorInvert[1] = false;
  motorInvert[2] = false;
  motorInvert[3] = false;

  Serial.println("✓ Конфигурация сброшена к дефолту");
}

String getConfigJSON() {
  String json = "{\"mapping\":[";
  for (int i = 0; i < 4; i++) {
    json += String(motorMapping[i]);
    if (i < 3) json += ",";
  }
  json += "],\"invert\":[";
  for (int i = 0; i < 4; i++) {
    json += motorInvert[i] ? "true" : "false";
    if (i < 3) json += ",";
  }
  json += "],\"omniMode\":";
  json += omniMode ? "true" : "false";
  json += "}";
  return json;
}

// ==================== ФУНКЦИИ УПРАВЛЕНИЯ МОТОРАМИ ====================

// Установить скорость и направление для одного ФИЗИЧЕСКОГО мотора
void setPhysicalMotor(int motorNum, int speed) {
  // speed: -255 до 255 (отрицательное = назад, положительное = вперед, 0 = стоп)

  int pwmChannel, pinD0, pinD1;

  switch(motorNum) {
    case 1:
      pwmChannel = PWM_CHANNEL_M1;
      pinD0 = MOTOR1_D0;
      pinD1 = MOTOR1_D1;
      break;
    case 2:
      pwmChannel = PWM_CHANNEL_M2;
      pinD0 = MOTOR2_D0;
      pinD1 = MOTOR2_D1;
      break;
    case 3:
      pwmChannel = PWM_CHANNEL_M3;
      pinD0 = MOTOR3_D0;
      pinD1 = MOTOR3_D1;
      break;
    case 4:
      pwmChannel = PWM_CHANNEL_M4;
      pinD0 = MOTOR4_D0;
      pinD1 = MOTOR4_D1;
      break;
    default:
      return;
  }

  if (speed == 0) {
    // Холостой ход (по таблице TA6586)
    digitalWrite(pinD1, LOW);
    ledcWrite(pwmChannel, 0);
  } else if (speed > 0) {
    // Вперёд: D0 = HIGH/PWM, D1 = LOW (по таблице TA6586)
    digitalWrite(pinD1, LOW);
    delayMicroseconds(10);
    ledcWrite(pwmChannel, abs(speed));
  } else {
    // Назад: D0 = LOW/PWM, D1 = HIGH (по таблице TA6586)
    // LOW/PWM означает ИНВЕРТИРОВАННЫЙ PWM: больше скорость = меньше duty cycle!
    int invertedPWM = 255 - abs(speed);
    digitalWrite(pinD1, HIGH);
    delayMicroseconds(10);
    ledcWrite(pwmChannel, invertedPWM);
  }
}

// Установить скорость для ЛОГИЧЕСКОГО мотора (с учетом маппинга и инверсии)
void setMotor(int logicalMotor, int speed) {
  // logicalMotor: 1-4 (логические позиции)
  // speed: -255 до 255

  if (logicalMotor < 1 || logicalMotor > 4) return;

  int index = logicalMotor - 1;  // Преобразовать в индекс массива (0-3)
  int physicalMotor = motorMapping[index];

  // Применить инверсию если включена
  if (motorInvert[index]) {
    speed = -speed;
  }

  setPhysicalMotor(physicalMotor, speed);
}

// Остановить все моторы
void stopAllMotors() {
  setMotor(1, 0);
  setMotor(2, 0);
  setMotor(3, 0);
  setMotor(4, 0);
}

// ==================== ФУНКЦИИ ДВИЖЕНИЯ OMNI-РОБОТА ====================
// Предполагается X-конфигурация колес (смотря сверху):
//     M1 ↗  ↖ M2
//         ╲╱
//         ╱╲
//     M3 ↙  ↘ M4

void moveForward() {
  setMotor(1, currentSpeed);
  setMotor(2, currentSpeed);
  setMotor(3, currentSpeed);
  setMotor(4, currentSpeed);
}

void moveBackward() {
  setMotor(1, -currentSpeed);
  setMotor(2, -currentSpeed);
  setMotor(3, -currentSpeed);
  setMotor(4, -currentSpeed);
}

void moveLeft() {
  setMotor(1, -currentSpeed);
  setMotor(2, currentSpeed);
  setMotor(3, currentSpeed);
  setMotor(4, -currentSpeed);
}

void moveRight() {
  setMotor(1, currentSpeed);
  setMotor(2, -currentSpeed);
  setMotor(3, -currentSpeed);
  setMotor(4, currentSpeed);
}

void rotateLeft() {
  setMotor(1, -currentSpeed);
  setMotor(2, currentSpeed);
  setMotor(3, -currentSpeed);
  setMotor(4, currentSpeed);
}

void rotateRight() {
  setMotor(1, currentSpeed);
  setMotor(2, -currentSpeed);
  setMotor(3, currentSpeed);
  setMotor(4, -currentSpeed);
}

void moveDiagonalForwardLeft() {
  setMotor(1, 0);
  setMotor(2, currentSpeed);
  setMotor(3, currentSpeed);
  setMotor(4, 0);
}

void moveDiagonalForwardRight() {
  setMotor(1, currentSpeed);
  setMotor(2, 0);
  setMotor(3, 0);
  setMotor(4, currentSpeed);
}

void moveDiagonalBackwardLeft() {
  setMotor(1, -currentSpeed);
  setMotor(2, 0);
  setMotor(3, 0);
  setMotor(4, -currentSpeed);
}

void moveDiagonalBackwardRight() {
  setMotor(1, 0);
  setMotor(2, -currentSpeed);
  setMotor(3, -currentSpeed);
  setMotor(4, 0);
}

// ==================== WEBSOCKET ОБРАБОТЧИКИ ====================

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String command = (char*)data;

    Serial.println("Команда: " + command);

    // Команды управления
    if (command == "forward") {
      moveForward();
    } else if (command == "backward") {
      moveBackward();
    } else if (command == "left") {
      moveLeft();
    } else if (command == "right") {
      moveRight();
    } else if (command == "rotate_left") {
      rotateLeft();
    } else if (command == "rotate_right") {
      rotateRight();
    } else if (command == "diag_fl") {
      moveDiagonalForwardLeft();
    } else if (command == "diag_fr") {
      moveDiagonalForwardRight();
    } else if (command == "diag_bl") {
      moveDiagonalBackwardLeft();
    } else if (command == "diag_br") {
      moveDiagonalBackwardRight();
    } else if (command == "stop") {
      stopAllMotors();
    } else if (command == "mode_omni") {
      omniMode = true;
      Serial.println("✓ Режим: Omni (strafe)");
    } else if (command == "mode_tank") {
      omniMode = false;
      Serial.println("✓ Режим: Tank (rotation)");
    }
    // Команды калибровки - тест по ЛОГИЧЕСКОЙ позиции (с учетом маппинга)
    else if (command.startsWith("test_")) {
      int pos = command.substring(5, 6).toInt();  // test_0_fwd -> 0
      String action = command.substring(7);       // fwd/bwd/stop

      if (pos >= 0 && pos < 4) {
        int logicalMotor = pos + 1;  // 0->1, 1->2, 2->3, 3->4
        if (action == "fwd") {
          setMotor(logicalMotor, currentSpeed);
        } else if (action == "bwd") {
          setMotor(logicalMotor, -currentSpeed);
        } else if (action == "stop") {
          setMotor(logicalMotor, 0);
        }
      }
    }
    // Изменение скорости
    else if (command.startsWith("speed:")) {
      int newSpeed = command.substring(6).toInt();
      if (newSpeed >= 0 && newSpeed <= 255) {
        currentSpeed = newSpeed;
        Serial.println("Скорость изменена на: " + String(currentSpeed));
      }
    }
    // Управление джойстиком: "joy:x:y" где x,y от -255 до 255
    else if (command.startsWith("joy:")) {
      int firstColon = command.indexOf(':', 4);
      int joyX = command.substring(4, firstColon).toInt();
      int joyY = command.substring(firstColon + 1).toInt();

      // X-конфигурация омни-платформы с двумя режимами
      //     M1 ↗  ↖ M2
      //         ╲╱
      //         ╱╲
      //     M3 ↙  ↘ M4

      int m1, m2, m3, m4;

      if (omniMode) {
        // OMNI MODE: X = стрейф влево/вправо, Y = вперёд/назад
        // Формулы: M1=Y+X, M2=Y-X, M3=Y+X, M4=Y-X
        m1 = constrain(joyY + joyX, -255, 255);
        m2 = constrain(joyY - joyX, -255, 255);
        m3 = constrain(joyY + joyX, -255, 255);
        m4 = constrain(joyY - joyX, -255, 255);
      } else {
        // TANK MODE: X = разворот влево/вправо, Y = вперёд/назад
        // Формулы: M1=Y-X, M2=Y+X, M3=Y-X, M4=Y+X
        m1 = constrain(joyY - joyX, -255, 255);
        m2 = constrain(joyY + joyX, -255, 255);
        m3 = constrain(joyY - joyX, -255, 255);
        m4 = constrain(joyY + joyX, -255, 255);
      }

      setMotor(1, m1);
      setMotor(2, m2);
      setMotor(3, m3);
      setMotor(4, m4);
    }
    // Команды настройки
    else if (command == "get_config") {
      ws.textAll(getConfigJSON());
    } else if (command == "save_config") {
      saveConfig();
      ws.textAll("{\"status\":\"saved\"}");
    } else if (command == "reset_config") {
      resetConfig();
      ws.textAll(getConfigJSON());
    }
    // Установка маппинга: "set_map:0:2" = логическая_позиция:физический_мотор
    else if (command.startsWith("set_map:")) {
      int firstColon = command.indexOf(':', 8);
      int logicalPos = command.substring(8, firstColon).toInt();
      int physicalMotor = command.substring(firstColon + 1).toInt();

      if (logicalPos >= 0 && logicalPos < 4 && physicalMotor >= 1 && physicalMotor <= 4) {
        motorMapping[logicalPos] = physicalMotor;
        Serial.printf("Маппинг установлен: позиция %d -> мотор %d\n", logicalPos, physicalMotor);
      }
    }
    // Установка инверсии: "set_inv:0:true"
    else if (command.startsWith("set_inv:")) {
      int firstColon = command.indexOf(':', 8);
      int logicalPos = command.substring(8, firstColon).toInt();
      String value = command.substring(firstColon + 1);

      if (logicalPos >= 0 && logicalPos < 4) {
        motorInvert[logicalPos] = (value == "true");
        Serial.printf("Инверсия установлена: позиция %d = %s\n", logicalPos, value.c_str());
      }
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket клиент #%u подключен\n", client->id());
      // Отправить текущую конфигурацию при подключении
      client->text(getConfigJSON());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket клиент #%u отключен\n", client->id());
      stopAllMotors(); // Остановить при отключении
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// ==================== HTML ИНТЕРФЕЙС ====================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Omni Robot Control</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #f8fafc;
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 700px;
      margin: 0 auto;
      background: white;
      border-radius: 12px;
      box-shadow: 0 1px 3px rgba(0,0,0,0.1);
      border: 1px solid #e2e8f0;
      overflow: hidden;
    }
    .header {
      background: white;
      border-bottom: 1px solid #e2e8f0;
      padding: 20px;
      text-align: center;
    }
    .header h1 {
      font-size: 20px;
      margin-bottom: 8px;
      color: #0f172a;
      font-weight: 600;
    }
    .status {
      font-size: 13px;
      font-weight: 500;
    }
    .status.connected { color: #10b981; }
    .status.disconnected { color: #64748b; }

    .tabs {
      display: flex;
      background: #f8fafc;
      border-bottom: 1px solid #e2e8f0;
    }
    .tab {
      flex: 1;
      padding: 14px;
      text-align: center;
      cursor: pointer;
      border: none;
      background: none;
      font-size: 14px;
      font-weight: 500;
      color: #64748b;
      transition: all 0.2s;
    }
    .tab.active {
      background: white;
      color: #3b82f6;
      border-bottom: 2px solid #3b82f6;
    }

    .mode-btn {
      padding: 10px 20px;
      border: none;
      background: transparent;
      color: #64748b;
      font-size: 14px;
      font-weight: 500;
      cursor: pointer;
      border-radius: 6px;
      transition: all 0.2s;
    }
    .mode-btn.active {
      background: white;
      color: #3b82f6;
      box-shadow: 0 1px 3px rgba(0,0,0,0.1);
    }

    .tab-content {
      display: none;
      padding: 30px 20px;
      max-height: 75vh;
      overflow-y: auto;
    }
    .tab-content.active {
      display: block;
    }

    .speed-control {
      margin-bottom: 20px;
      text-align: center;
    }
    .speed-control label {
      display: block;
      font-size: 14px;
      font-weight: 500;
      margin-bottom: 10px;
      color: #475569;
    }
    .speed-slider {
      width: 100%;
      margin: 10px 0;
      height: 6px;
      border-radius: 3px;
      background: #e2e8f0;
      outline: none;
      -webkit-appearance: none;
    }
    .speed-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #3b82f6;
      cursor: pointer;
      border: 2px solid white;
      box-shadow: 0 1px 3px rgba(0,0,0,0.2);
    }
    .speed-slider::-moz-range-thumb {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #3b82f6;
      cursor: pointer;
      border: 2px solid white;
      box-shadow: 0 1px 3px rgba(0,0,0,0.2);
    }
    .speed-value {
      font-size: 28px;
      font-weight: 600;
      color: #3b82f6;
    }

    .joystick-layout {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
      margin-bottom: 20px;
    }

    .control-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
    }
    .btn {
      padding: 20px;
      font-size: 24px;
      border: 1px solid #e2e8f0;
      border-radius: 8px;
      cursor: pointer;
      background: white;
      color: #3b82f6;
      transition: all 0.15s;
      user-select: none;
      -webkit-user-select: none;
      -webkit-touch-callout: none;
      font-weight: 500;
      box-shadow: 0 1px 2px rgba(0,0,0,0.05);
    }
    .btn:active {
      transform: scale(0.98);
      background: #eff6ff;
      border-color: #3b82f6;
    }
    .btn.empty {
      background: transparent;
      cursor: default;
      border: none;
      box-shadow: none;
    }
    .btn.stop {
      background: #ef4444;
      color: white;
      border-color: #ef4444;
      grid-column: 2;
    }
    .btn.stop:active {
      background: #dc2626;
      border-color: #dc2626;
    }

    .rotate-buttons {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      height: 100%;
    }

    .rotate-buttons .btn {
      font-size: 18px;
    }

    .emergency-stop {
      width: 100%;
      padding: 18px;
      font-size: 16px;
      font-weight: 600;
      background: #ef4444;
      color: white;
      border: 1px solid #ef4444;
      border-radius: 8px;
      cursor: pointer;
      margin-top: 20px;
      box-shadow: 0 1px 3px rgba(239,68,68,0.3);
      transition: all 0.15s;
    }
    .emergency-stop:active {
      background: #dc2626;
      border-color: #dc2626;
      transform: scale(0.98);
    }

    /* Калибровка - визуальный квадрат */
    .info-box {
      background: #f0f9ff;
      border: 1px solid #bae6fd;
      padding: 14px;
      margin-bottom: 20px;
      border-radius: 8px;
    }
    .info-box p {
      font-size: 13px;
      color: #0369a1;
      line-height: 1.6;
      margin-bottom: 6px;
    }
    .info-box p:last-child {
      margin-bottom: 0;
    }

    .robot-visual {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
      margin-bottom: 24px;
      padding: 16px;
      background: #f8fafc;
      border-radius: 8px;
      border: 1px solid #e2e8f0;
    }

    .motor-corner {
      background: white;
      border-radius: 8px;
      padding: 14px;
      border: 1px solid #e2e8f0;
      box-shadow: 0 1px 2px rgba(0,0,0,0.05);
    }

    .corner-header {
      text-align: center;
      margin-bottom: 12px;
      padding-bottom: 10px;
      border-bottom: 1px solid #e2e8f0;
    }

    .corner-header h3 {
      font-size: 13px;
      color: #475569;
      margin-bottom: 4px;
      font-weight: 500;
    }

    .corner-header .icon {
      font-size: 24px;
      margin-bottom: 4px;
    }

    .test-controls {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 6px;
      margin-bottom: 12px;
    }

    .test-controls .btn {
      padding: 10px 6px;
      font-size: 16px;
    }

    .btn.forward {
      background: white;
      color: #10b981;
      border-color: #d1fae5;
    }
    .btn.forward:active {
      background: #f0fdf4;
      border-color: #10b981;
    }
    .btn.backward {
      background: white;
      color: #f59e0b;
      border-color: #fed7aa;
    }
    .btn.backward:active {
      background: #fffbeb;
      border-color: #f59e0b;
    }
    .btn.test-stop {
      background: #ef4444;
      color: white;
      border-color: #ef4444;
    }
    .btn.test-stop:active {
      background: #dc2626;
      border-color: #dc2626;
    }

    .corner-settings {
      margin-top: 10px;
    }

    .setting-item {
      margin-bottom: 8px;
    }

    .setting-item label {
      display: block;
      font-size: 12px;
      color: #64748b;
      margin-bottom: 4px;
      font-weight: 500;
    }

    .setting-item select {
      width: 100%;
      padding: 8px;
      border: 1px solid #e2e8f0;
      border-radius: 6px;
      font-size: 13px;
      background: white;
      color: #475569;
      cursor: pointer;
      transition: all 0.15s;
    }

    .setting-item select:focus {
      outline: none;
      border-color: #3b82f6;
      box-shadow: 0 0 0 3px rgba(59,130,246,0.1);
    }

    .invert-check {
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 8px;
      background: #f8fafc;
      border-radius: 6px;
      border: 1px solid #e2e8f0;
    }

    .invert-check input[type="checkbox"] {
      width: 16px;
      height: 16px;
      margin-right: 8px;
      cursor: pointer;
      accent-color: #3b82f6;
    }

    .invert-check label {
      font-size: 12px;
      color: #475569;
      cursor: pointer;
      margin: 0;
      font-weight: 500;
    }

    .action-buttons {
      display: grid;
      grid-template-columns: 2fr 1fr;
      gap: 10px;
      margin-top: 20px;
    }

    .action-buttons .btn {
      padding: 14px;
      font-size: 14px;
    }

    .btn.save {
      background: #3b82f6;
      color: white;
      border-color: #3b82f6;
    }
    .btn.save:active {
      background: #2563eb;
      border-color: #2563eb;
    }
    .btn.reset {
      background: white;
      color: #ef4444;
      border-color: #fecaca;
    }
    .btn.reset:active {
      background: #fef2f2;
      border-color: #ef4444;
    }

    @media (max-width: 600px) {
      .robot-visual {
        gap: 15px;
        padding: 15px;
      }
      .motor-corner {
        padding: 12px;
      }
      .corner-header .icon {
        font-size: 24px;
      }
      .test-controls .btn {
        padding: 10px 5px;
        font-size: 12px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🤖 Omni Robot Control</h1>
      <div class="status" id="status">Подключение...</div>
    </div>

    <div class="tabs">
      <button class="tab active" onclick="switchTab(0)">Управление</button>
      <button class="tab" onclick="switchTab(1)">Калибровка</button>
    </div>

    <!-- Вкладка 1: Управление -->
    <div class="tab-content active" id="tab-control">
      <!-- Переключатель режимов управления и типа -->
      <div style="text-align:center; margin-bottom:20px;">
        <div style="display:inline-flex; background:#f1f5f9; border-radius:8px; padding:4px; margin-bottom:10px;">
          <button id="modeJoystick" class="mode-btn active" onclick="switchMode('joystick')">🕹️ Джойстик</button>
          <button id="modeButtons" class="mode-btn" onclick="switchMode('buttons')">🎮 Кнопки</button>
        </div>
        <br>
        <div style="display:inline-flex; background:#e0f2fe; border-radius:8px; padding:4px;">
          <button id="driveOmni" class="mode-btn active" onclick="switchDriveMode('omni')">🔄 Omni (Strafe)</button>
          <button id="driveTank" class="mode-btn" onclick="switchDriveMode('tank')">🎯 Tank (Rotation)</button>
        </div>
      </div>

      <div class="speed-control">
        <label>Скорость</label>
        <input type="range" class="speed-slider" min="0" max="255" value="200" id="speedSlider" oninput="updateSpeed()">
        <div class="speed-value" id="speedValue">200</div>
      </div>

      <!-- Режим джойстика -->
      <div id="joystick-mode" class="control-mode">
        <div style="text-align:center; margin-bottom:10px; color:#64748b; font-size:13px;">
          🕹️ Вверх/Вниз: движение • Влево/Вправо: <span id="joystickModeText">стрейф</span>
        </div>
        <div style="display:grid; grid-template-columns:1fr 1fr; gap:15px; margin-bottom:20px;">
          <!-- Джойстик слева -->
          <div>
            <h3 style="text-align:center; margin-bottom:10px; color:#475569; font-weight:500; font-size:14px;">Джойстик</h3>
            <div style="position:relative; width:100%; padding-bottom:100%; background:#f8fafc; border-radius:12px; border:2px solid #e2e8f0;">
              <canvas id="joystickCanvas" style="position:absolute; width:100%; height:100%; touch-action:none;"></canvas>
            </div>
          </div>

          <!-- Кнопки влево/вправо справа -->
          <div>
            <h3 style="text-align:center; margin-bottom:10px; color:#475569; font-weight:500; font-size:14px;" id="joystickSideLabel">Стрейф</h3>
            <div class="rotate-buttons">
              <button class="btn" ontouchstart="sendCommand('left')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('left')" onmouseup="sendCommand('stop')">⟲</button>
              <button class="btn" ontouchstart="sendCommand('right')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('right')" onmouseup="sendCommand('stop')">⟳</button>
            </div>
          </div>
        </div>
      </div>

      <!-- Режим кнопок -->
      <div id="buttons-mode" class="control-mode" style="display:none;">
        <div style="text-align:center; margin-bottom:10px; color:#64748b; font-size:13px;">
          🎮 ⬆️⬇️ движение • ⬅️➡️ <span id="buttonsModeText">разворот</span>
        </div>
        <div class="joystick-layout">
        <!-- Левая половина: направления -->
        <div>
          <h3 style="text-align:center; margin-bottom:10px; color:#475569; font-weight:500; font-size:14px;">Движение</h3>
          <div class="control-grid">
            <div class="btn empty"></div>
            <button class="btn" ontouchstart="sendCommand('forward')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('forward')" onmouseup="sendCommand('stop')">⬆️</button>
            <div class="btn empty"></div>

            <button class="btn" ontouchstart="sendCommand('rotate_left')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('rotate_left')" onmouseup="sendCommand('stop')">⬅️</button>
            <button class="btn stop" onclick="sendCommand('stop')">⏹️</button>
            <button class="btn" ontouchstart="sendCommand('rotate_right')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('rotate_right')" onmouseup="sendCommand('stop')">➡️</button>

            <div class="btn empty"></div>
            <button class="btn" ontouchstart="sendCommand('backward')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('backward')" onmouseup="sendCommand('stop')">⬇️</button>
            <div class="btn empty"></div>
          </div>
        </div>

        <!-- Правая половина: стрейф/разворот -->
        <div>
          <h3 style="text-align:center; margin-bottom:10px; color:#475569; font-weight:500; font-size:14px;" id="buttonsSideLabel">Разворот</h3>
          <div class="rotate-buttons">
            <button class="btn" ontouchstart="sendCommand('left')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('left')" onmouseup="sendCommand('stop')">⟲</button>
            <button class="btn" ontouchstart="sendCommand('right')" ontouchend="sendCommand('stop')" onmousedown="sendCommand('right')" onmouseup="sendCommand('stop')">⟳</button>
          </div>
        </div>
      </div>
      </div>

      <button class="emergency-stop" onclick="sendCommand('stop')">🛑 АВАРИЙНЫЙ СТОП</button>
    </div>

    <!-- Вкладка 2: Калибровка -->
    <div class="tab-content" id="tab-calibration">
      <div class="speed-control">
        <label>Скорость тестирования</label>
        <input type="range" class="speed-slider" min="0" max="255" value="200" id="speedSlider2" oninput="updateSpeed2()">
        <div class="speed-value" id="speedValue2">200</div>
      </div>

      <div class="info-box">
        <p><strong>Инструкция:</strong></p>
        <p>1. Нажми кнопки теста для каждого угла</p>
        <p>2. Выбери правильный физический мотор из списка</p>
        <p>3. Поставь галочку "Реверс" если мотор крутится наоборот</p>
        <p>4. Нажми "Сохранить" когда все настроено</p>
      </div>

      <div class="robot-visual">
        <!-- Передний-левый (M2) -->
        <div class="motor-corner">
          <div class="corner-header">
            <div class="icon">↖️</div>
            <h3>Передний-левый</h3>
          </div>
          <div class="test-controls">
            <button class="btn forward" ontouchstart="sendCommand('test_1_fwd')" ontouchend="sendCommand('test_1_stop')" onmousedown="sendCommand('test_1_fwd')" onmouseup="sendCommand('test_1_stop')">⬆️</button>
            <button class="btn test-stop" onclick="sendCommand('test_1_stop')">⏹️</button>
            <button class="btn backward" ontouchstart="sendCommand('test_1_bwd')" ontouchend="sendCommand('test_1_stop')" onmousedown="sendCommand('test_1_bwd')" onmouseup="sendCommand('test_1_stop')">⬇️</button>
          </div>
          <div class="corner-settings">
            <div class="setting-item">
              <label>Физический мотор:</label>
              <select id="map1" onchange="updateMapping(1)">
                <option value="1">Мотор 1 (32,33)</option>
                <option value="2">Мотор 2 (25,26)</option>
                <option value="3">Мотор 3 (19,18)</option>
                <option value="4">Мотор 4 (17,16)</option>
              </select>
            </div>
            <div class="invert-check">
              <input type="checkbox" id="inv1" onchange="updateInvert(1)">
              <label for="inv1">Реверс</label>
            </div>
          </div>
        </div>

        <!-- Передний-правый (M1) -->
        <div class="motor-corner">
          <div class="corner-header">
            <div class="icon">↗️</div>
            <h3>Передний-правый</h3>
          </div>
          <div class="test-controls">
            <button class="btn forward" ontouchstart="sendCommand('test_0_fwd')" ontouchend="sendCommand('test_0_stop')" onmousedown="sendCommand('test_0_fwd')" onmouseup="sendCommand('test_0_stop')">⬆️</button>
            <button class="btn test-stop" onclick="sendCommand('test_0_stop')">⏹️</button>
            <button class="btn backward" ontouchstart="sendCommand('test_0_bwd')" ontouchend="sendCommand('test_0_stop')" onmousedown="sendCommand('test_0_bwd')" onmouseup="sendCommand('test_0_stop')">⬇️</button>
          </div>
          <div class="corner-settings">
            <div class="setting-item">
              <label>Физический мотор:</label>
              <select id="map0" onchange="updateMapping(0)">
                <option value="1">Мотор 1 (32,33)</option>
                <option value="2">Мотор 2 (25,26)</option>
                <option value="3">Мотор 3 (19,18)</option>
                <option value="4">Мотор 4 (17,16)</option>
              </select>
            </div>
            <div class="invert-check">
              <input type="checkbox" id="inv0" onchange="updateInvert(0)">
              <label for="inv0">Реверс</label>
            </div>
          </div>
        </div>

        <!-- Задний-левый (M3) -->
        <div class="motor-corner">
          <div class="corner-header">
            <div class="icon">↙️</div>
            <h3>Задний-левый</h3>
          </div>
          <div class="test-controls">
            <button class="btn forward" ontouchstart="sendCommand('test_2_fwd')" ontouchend="sendCommand('test_2_stop')" onmousedown="sendCommand('test_2_fwd')" onmouseup="sendCommand('test_2_stop')">⬆️</button>
            <button class="btn test-stop" onclick="sendCommand('test_2_stop')">⏹️</button>
            <button class="btn backward" ontouchstart="sendCommand('test_2_bwd')" ontouchend="sendCommand('test_2_stop')" onmousedown="sendCommand('test_2_bwd')" onmouseup="sendCommand('test_2_stop')">⬇️</button>
          </div>
          <div class="corner-settings">
            <div class="setting-item">
              <label>Физический мотор:</label>
              <select id="map2" onchange="updateMapping(2)">
                <option value="1">Мотор 1 (32,33)</option>
                <option value="2">Мотор 2 (25,26)</option>
                <option value="3">Мотор 3 (19,18)</option>
                <option value="4">Мотор 4 (17,16)</option>
              </select>
            </div>
            <div class="invert-check">
              <input type="checkbox" id="inv2" onchange="updateInvert(2)">
              <label for="inv2">Реверс</label>
            </div>
          </div>
        </div>

        <!-- Задний-правый (M4) -->
        <div class="motor-corner">
          <div class="corner-header">
            <div class="icon">↘️</div>
            <h3>Задний-правый</h3>
          </div>
          <div class="test-controls">
            <button class="btn forward" ontouchstart="sendCommand('test_3_fwd')" ontouchend="sendCommand('test_3_stop')" onmousedown="sendCommand('test_3_fwd')" onmouseup="sendCommand('test_3_stop')">⬆️</button>
            <button class="btn test-stop" onclick="sendCommand('test_3_stop')">⏹️</button>
            <button class="btn backward" ontouchstart="sendCommand('test_3_bwd')" ontouchend="sendCommand('test_3_stop')" onmousedown="sendCommand('test_3_bwd')" onmouseup="sendCommand('test_3_stop')">⬇️</button>
          </div>
          <div class="corner-settings">
            <div class="setting-item">
              <label>Физический мотор:</label>
              <select id="map3" onchange="updateMapping(3)">
                <option value="1">Мотор 1 (32,33)</option>
                <option value="2">Мотор 2 (25,26)</option>
                <option value="3">Мотор 3 (19,18)</option>
                <option value="4">Мотор 4 (17,16)</option>
              </select>
            </div>
            <div class="invert-check">
              <input type="checkbox" id="inv3" onchange="updateInvert(3)">
              <label for="inv3">Реверс</label>
            </div>
          </div>
        </div>
      </div>

      <div class="action-buttons">
        <button class="btn save" onclick="saveSettings()">💾 Сохранить настройки</button>
        <button class="btn reset" onclick="resetSettings()">🔄 Сброс</button>
      </div>
    </div>
  </div>

  <script>
    let ws;
    const statusEl = document.getElementById('status');
    let currentDriveMode = 'omni';  // 'omni' or 'tank'

    function initWebSocket() {
      ws = new WebSocket('ws://' + window.location.hostname + '/ws');

      ws.onopen = function() {
        statusEl.textContent = '✓ Подключено';
        statusEl.className = 'status connected';
        sendCommand('get_config');
      };

      ws.onclose = function() {
        statusEl.textContent = '✗ Отключено';
        statusEl.className = 'status disconnected';
        setTimeout(initWebSocket, 2000);
      };

      ws.onerror = function() {
        statusEl.textContent = '✗ Ошибка подключения';
        statusEl.className = 'status disconnected';
      };

      ws.onmessage = function(event) {
        try {
          const data = JSON.parse(event.data);
          if (data.mapping && data.invert) {
            loadConfigToUI(data);
          } else if (data.status === 'saved') {
            alert('💾 Настройки сохранены в память ESP32!');
          }
        } catch (e) {
          console.log('Получено сообщение:', event.data);
        }
      };
    }

    function sendCommand(cmd) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(cmd);
      }
    }

    function updateSpeed() {
      const speed = document.getElementById('speedSlider').value;
      document.getElementById('speedValue').textContent = speed;
      document.getElementById('speedSlider2').value = speed;
      document.getElementById('speedValue2').textContent = speed;
      sendCommand('speed:' + speed);
    }

    function updateSpeed2() {
      const speed = document.getElementById('speedSlider2').value;
      document.getElementById('speedValue2').textContent = speed;
      document.getElementById('speedSlider').value = speed;
      document.getElementById('speedValue').textContent = speed;
      sendCommand('speed:' + speed);
    }

    function switchTab(index) {
      const tabs = document.querySelectorAll('.tab');
      const contents = document.querySelectorAll('.tab-content');

      tabs.forEach((tab, i) => {
        tab.classList.toggle('active', i === index);
      });

      contents.forEach((content, i) => {
        content.classList.toggle('active', i === index);
      });

      sendCommand('stop');
    }

    function loadConfigToUI(config) {
      for (let i = 0; i < 4; i++) {
        document.getElementById('map' + i).value = config.mapping[i];
        document.getElementById('inv' + i).checked = config.invert[i];
      }

      // Load drive mode
      if (config.omniMode !== undefined) {
        currentDriveMode = config.omniMode ? 'omni' : 'tank';
        updateDriveModeUI();
      }
    }

    function updateMapping(pos) {
      const value = document.getElementById('map' + pos).value;
      sendCommand('set_map:' + pos + ':' + value);
    }

    function updateInvert(pos) {
      const value = document.getElementById('inv' + pos).checked;
      sendCommand('set_inv:' + pos + ':' + value);
    }

    function saveSettings() {
      // Применить все текущие настройки
      for (let i = 0; i < 4; i++) {
        updateMapping(i);
        updateInvert(i);
      }
      // Отправить текущий режим вождения
      sendCommand(currentDriveMode === 'omni' ? 'mode_omni' : 'mode_tank');
      // Сохранить в EEPROM
      sendCommand('save_config');
    }

    function resetSettings() {
      if (confirm('Сбросить все настройки к дефолту?')) {
        sendCommand('reset_config');
        alert('🔄 Настройки сброшены! Не забудь сохранить.');
      }
    }

    // ========== ПЕРЕКЛЮЧЕНИЕ РЕЖИМА ВОЖДЕНИЯ ==========
    function switchDriveMode(mode) {
      currentDriveMode = mode;
      sendCommand(mode === 'omni' ? 'mode_omni' : 'mode_tank');
      updateDriveModeUI();
    }

    function updateDriveModeUI() {
      const btnOmni = document.getElementById('driveOmni');
      const btnTank = document.getElementById('driveTank');
      const joystickModeText = document.getElementById('joystickModeText');
      const buttonsModeText = document.getElementById('buttonsModeText');
      const joystickSideLabel = document.getElementById('joystickSideLabel');
      const buttonsSideLabel = document.getElementById('buttonsSideLabel');

      if (currentDriveMode === 'omni') {
        btnOmni.classList.add('active');
        btnTank.classList.remove('active');
        joystickModeText.textContent = 'стрейф';
        buttonsModeText.textContent = 'стрейф';
        joystickSideLabel.textContent = 'Стрейф';
        buttonsSideLabel.textContent = 'Стрейф';
      } else {
        btnOmni.classList.remove('active');
        btnTank.classList.add('active');
        joystickModeText.textContent = 'разворот';
        buttonsModeText.textContent = 'разворот';
        joystickSideLabel.textContent = 'Разворот';
        buttonsSideLabel.textContent = 'Разворот';
      }
    }

    document.addEventListener('selectstart', function(e) {
      e.preventDefault();
    });

    // ========== ДЖОЙСТИК ==========
    let joystickActive = false;
    let joystickX = 0;
    let joystickY = 0;

    function initJoystick() {
      const canvas = document.getElementById('joystickCanvas');
      if (!canvas) return;

      const ctx = canvas.getContext('2d');
      const rect = canvas.getBoundingClientRect();
      canvas.width = rect.width;
      canvas.height = rect.height;

      const centerX = canvas.width / 2;
      const centerY = canvas.height / 2;
      const maxRadius = Math.min(canvas.width, canvas.height) / 2 - 20;

      function drawJoystick() {
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // Внешний круг
        ctx.beginPath();
        ctx.arc(centerX, centerY, maxRadius, 0, 2 * Math.PI);
        ctx.strokeStyle = '#e2e8f0';
        ctx.lineWidth = 2;
        ctx.stroke();

        // Центр
        ctx.beginPath();
        ctx.arc(centerX, centerY, 5, 0, 2 * Math.PI);
        ctx.fillStyle = '#cbd5e1';
        ctx.fill();

        // Стик
        const stickX = centerX + joystickX * maxRadius / 255;
        const stickY = centerY + joystickY * maxRadius / 255;
        ctx.beginPath();
        ctx.arc(stickX, stickY, 30, 0, 2 * Math.PI);
        ctx.fillStyle = joystickActive ? '#3b82f6' : '#94a3b8';
        ctx.fill();
        ctx.strokeStyle = 'white';
        ctx.lineWidth = 3;
        ctx.stroke();
      }

      function handleMove(clientX, clientY) {
        const rect = canvas.getBoundingClientRect();
        const x = clientX - rect.left - centerX;
        const y = clientY - rect.top - centerY;

        const distance = Math.sqrt(x * x + y * y);
        const angle = Math.atan2(y, x);

        const clampedDistance = Math.min(distance, maxRadius);

        joystickX = Math.round((clampedDistance * Math.cos(angle) / maxRadius) * 255);
        joystickY = -Math.round((clampedDistance * Math.sin(angle) / maxRadius) * 255);  // Инвертируем Y

        drawJoystick();
        sendCommand('joy:' + joystickX + ':' + joystickY);
      }

      function handleEnd() {
        joystickActive = false;
        joystickX = 0;
        joystickY = 0;
        drawJoystick();
        sendCommand('stop');
      }

      // Touch events
      canvas.addEventListener('touchstart', (e) => {
        e.preventDefault();
        joystickActive = true;
        handleMove(e.touches[0].clientX, e.touches[0].clientY);
      });

      canvas.addEventListener('touchmove', (e) => {
        e.preventDefault();
        if (joystickActive) {
          handleMove(e.touches[0].clientX, e.touches[0].clientY);
        }
      });

      canvas.addEventListener('touchend', (e) => {
        e.preventDefault();
        handleEnd();
      });

      // Mouse events
      canvas.addEventListener('mousedown', (e) => {
        joystickActive = true;
        handleMove(e.clientX, e.clientY);
      });

      canvas.addEventListener('mousemove', (e) => {
        if (joystickActive) {
          handleMove(e.clientX, e.clientY);
        }
      });

      canvas.addEventListener('mouseup', handleEnd);
      canvas.addEventListener('mouseleave', handleEnd);

      drawJoystick();
    }

    // ========== ПЕРЕКЛЮЧЕНИЕ РЕЖИМОВ ==========
    function switchMode(mode) {
      const joystickMode = document.getElementById('joystick-mode');
      const buttonsMode = document.getElementById('buttons-mode');
      const btnJoystick = document.getElementById('modeJoystick');
      const btnButtons = document.getElementById('modeButtons');

      if (mode === 'joystick') {
        joystickMode.style.display = 'block';
        buttonsMode.style.display = 'none';
        btnJoystick.classList.add('active');
        btnButtons.classList.remove('active');
        setTimeout(initJoystick, 100);
      } else {
        joystickMode.style.display = 'none';
        buttonsMode.style.display = 'block';
        btnJoystick.classList.remove('active');
        btnButtons.classList.add('active');
      }
    }

    initWebSocket();
    setTimeout(() => {
      initJoystick();
    }, 500);
  </script>
</body>
</html>
)rawliteral";

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("   ESP32 Omni Robot Controller");
  Serial.println("=================================\n");

  Serial.println("TA6586 управление (по официальной таблице):");
  Serial.println("  Вперёд: D0=HIGH/PWM, D1=LOW");
  Serial.println("  Назад:  D0=LOW/PWM (инверсный), D1=HIGH");
  Serial.println("  Холостой: D0=LOW, D1=LOW\n");

  // Загрузить конфигурацию из памяти
  loadConfig();

  // Настройка пинов моторов
  pinMode(MOTOR1_D1, OUTPUT);
  pinMode(MOTOR2_D1, OUTPUT);
  pinMode(MOTOR3_D1, OUTPUT);
  pinMode(MOTOR4_D1, OUTPUT);

  // Настройка PWM каналов
  ledcSetup(PWM_CHANNEL_M1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_M2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_M3, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_M4, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(MOTOR1_D0, PWM_CHANNEL_M1);
  ledcAttachPin(MOTOR2_D0, PWM_CHANNEL_M2);
  ledcAttachPin(MOTOR3_D0, PWM_CHANNEL_M3);
  ledcAttachPin(MOTOR4_D0, PWM_CHANNEL_M4);

  // Остановить все моторы при старте
  stopAllMotors();

  Serial.println("✓ Моторы инициализированы");

  // Подключение к WiFi
  Serial.print("Подключение к WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi подключен!");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.localIP());
    Serial.print("Открой в браузере: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ Не удалось подключиться к WiFi");
    Serial.println("Проверь SSID и пароль");
  }

  // Настройка WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Главная страница
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  // Запуск сервера
  server.begin();
  Serial.println("✓ Веб-сервер запущен\n");
  Serial.println("=================================\n");
}

// ==================== LOOP ====================

void loop() {
  ws.cleanupClients();
  delay(10);
}
