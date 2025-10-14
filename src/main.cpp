#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ==================== BLE UUIDs ====================

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_COMMAND   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_JOYSTICK  "ca73b3ba-39f6-4ab3-91ae-186dc9577d99"
#define CHAR_UUID_SPEED     "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define CHAR_UUID_CONFIG    "d4e1f1a2-8b5c-4d3e-9f7a-6c8b5a4d3e2f"
#define CHAR_UUID_TEST      "a3b2c1d4-5e6f-7a8b-9c0d-1e2f3a4b5c6d"

// ==================== КОНФИГУРАЦИЯ ====================

// Пины моторов (TA6586 драйверы)
// Драйвер 1
#define MOTOR1_D0 32  // PWM для вперед
#define MOTOR1_D1 33  // Направление (LOW/HIGH)
#define MOTOR2_D0 25  // PWM для вперед
#define MOTOR2_D1 26  // Направление (LOW/HIGH)

// Драйвер 2
#define MOTOR3_D0 19
#define MOTOR3_D1 18
#define MOTOR4_D0 17
#define MOTOR4_D1 16

// PWM настройки
#define PWM_FREQ 5000
#define PWM_RES 8

// PWM каналы для каждого мотора
#define PWM_CHANNEL_M1 0
#define PWM_CHANNEL_M2 1
#define PWM_CHANNEL_M3 2
#define PWM_CHANNEL_M4 3

// ==================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ====================

BLEServer* pServer = NULL;
BLECharacteristic* pCharCommand = NULL;
BLECharacteristic* pCharJoystick = NULL;
BLECharacteristic* pCharSpeed = NULL;
BLECharacteristic* pCharConfig = NULL;
BLECharacteristic* pCharTest = NULL;

bool deviceConnected = false;
Preferences preferences;

// Текущая скорость (0-255)
int currentSpeed = 200;

// Режим управления: true = Omni (strafe), false = Tank (rotation)
bool omniMode = true;

// Конфигурация моторов
int motorMapping[4] = {1, 2, 3, 4};
bool motorInvert[4] = {false, false, false, false};

// ==================== ФУНКЦИИ РАБОТЫ С НАСТРОЙКАМИ ====================

void loadConfig() {
  preferences.begin("robot", true);

  for (int i = 0; i < 4; i++) {
    String key = "map" + String(i);
    motorMapping[i] = preferences.getInt(key.c_str(), i + 1);

    key = "inv" + String(i);
    motorInvert[i] = preferences.getBool(key.c_str(), false);
  }

  omniMode = preferences.getBool("omniMode", true);

  preferences.end();

  Serial.println("✓ Конфигурация загружена:");
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
  preferences.begin("robot", false);

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
  JsonDocument doc;
  JsonArray mapping = doc["mapping"].to<JsonArray>();
  JsonArray invert = doc["invert"].to<JsonArray>();

  for (int i = 0; i < 4; i++) {
    mapping.add(motorMapping[i]);
    invert.add(motorInvert[i]);
  }

  doc["omniMode"] = omniMode;

  String output;
  serializeJson(doc, output);
  return output;
}

// ==================== ФУНКЦИИ УПРАВЛЕНИЯ МОТОРАМИ ====================

void setPhysicalMotor(int motorNum, int speed) {
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

  if (speed > 0) {
    // Вперёд: D1=LOW, D0=PWM
    digitalWrite(pinD1, LOW);
    delayMicroseconds(10);
    ledcWrite(pwmChannel, abs(speed));
  } else if (speed < 0) {
    // Назад: D1=HIGH, D0=PWM (инверсный)
    digitalWrite(pinD1, HIGH);
    delayMicroseconds(10);
    ledcWrite(pwmChannel, 255 - abs(speed));
  } else {
    // Стоп
    digitalWrite(pinD1, LOW);
    ledcWrite(pwmChannel, 0);
  }
}

void setMotor(int logicalMotor, int speed) {
  if (logicalMotor < 1 || logicalMotor > 4) return;

  int logicalPos = logicalMotor - 1;
  int physicalMotor = motorMapping[logicalPos];

  if (motorInvert[logicalPos]) {
    speed = -speed;
  }

  setPhysicalMotor(physicalMotor, speed);
}

void stopAllMotors() {
  for (int i = 1; i <= 4; i++) {
    setPhysicalMotor(i, 0);
  }
}

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

void strafeLeft() {
  setMotor(1, -currentSpeed);
  setMotor(2, currentSpeed);
  setMotor(3, currentSpeed);
  setMotor(4, -currentSpeed);
}

void strafeRight() {
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

void handleJoystick(int8_t x, int8_t y) {
  // Преобразовать int8_t (-128..127) в -255..255
  int scaledX = map(x, -128, 127, 255, -255);  // Инвертирован X (право->лево)
  int scaledY = map(y, -128, 127, -255, 255);

  // X-конфигурация омни-платформы
  //     M1 ↗  ↖ M2
  //         ╲╱
  //         ╱╲
  //     M3 ↙  ↘ M4

  int m1, m2, m3, m4;

  if (omniMode) {
    // OMNI MODE: X = стрейф влево/вправо, Y = вперёд/назад
    // Формулы: M1=Y+X, M2=Y-X, M3=Y+X, M4=Y-X
    // При X=max: диагональное движение (strafe)
    m1 = constrain(scaledY + scaledX, -255, 255);
    m2 = constrain(scaledY - scaledX, -255, 255);
    m3 = constrain(scaledY + scaledX, -255, 255);
    m4 = constrain(scaledY - scaledX, -255, 255);
  } else {
    // TANK MODE: X = разворот влево/вправо, Y = вперёд/назад
    // Формулы: M1=Y+X, M2=Y-X, M3=Y-X, M4=Y+X
    // При X=max: разворот на месте (rotation)
    m1 = constrain(scaledY + scaledX, -255, 255);
    m2 = constrain(scaledY - scaledX, -255, 255);
    m3 = constrain(scaledY - scaledX, -255, 255);
    m4 = constrain(scaledY + scaledX, -255, 255);
  }

  setMotor(1, m1);
  setMotor(2, m2);
  setMotor(3, m3);
  setMotor(4, m4);
}

// ==================== BLE CALLBACKS ====================

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("✓ BLE устройство подключено");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    stopAllMotors();
    Serial.println("✗ BLE устройство отключено");
    delay(500);
    BLEDevice::startAdvertising();
  }
};

class CommandCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      String command = String(value.c_str());
      Serial.println("Команда: " + command);

      if (command == "forward") {
        moveForward();
      } else if (command == "backward") {
        moveBackward();
      } else if (command == "strafe_left") {
        strafeLeft();
      } else if (command == "strafe_right") {
        strafeRight();
      } else if (command == "rotate_left") {
        rotateLeft();
      } else if (command == "rotate_right") {
        rotateRight();
      } else if (command == "stop") {
        stopAllMotors();
      } else if (command == "mode_omni") {
        omniMode = true;
        Serial.println("✓ Режим: Omni (strafe)");
      } else if (command == "mode_tank") {
        omniMode = false;
        Serial.println("✓ Режим: Tank (rotation)");
      }
    }
  }
};

class JoystickCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    uint8_t* data = pCharacteristic->getData();
    size_t len = pCharacteristic->getLength();

    if (len == 2) {
      int8_t x = (int8_t)data[0];
      int8_t y = (int8_t)data[1];
      Serial.printf("Joystick: x=%d, y=%d\n", x, y);
      handleJoystick(x, y);
    }
  }
};

class SpeedCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    uint8_t* data = pCharacteristic->getData();
    if (pCharacteristic->getLength() == 1) {
      currentSpeed = data[0];
      Serial.printf("Скорость изменена на: %d\n", currentSpeed);
    }
  }
};

class ConfigCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      String command = String(value.c_str());
      Serial.println("Config команда: " + command);

      if (command == "save") {
        saveConfig();
        pCharConfig->setValue(getConfigJSON().c_str());
        pCharConfig->notify();
      } else if (command == "reset") {
        resetConfig();
        pCharConfig->setValue(getConfigJSON().c_str());
        pCharConfig->notify();
      } else if (command.startsWith("set_map:")) {
        int firstColon = command.indexOf(':', 8);
        int logicalPos = command.substring(8, firstColon).toInt();
        int physicalMotor = command.substring(firstColon + 1).toInt();

        if (logicalPos >= 0 && logicalPos < 4 && physicalMotor >= 1 && physicalMotor <= 4) {
          motorMapping[logicalPos] = physicalMotor;
          Serial.printf("Маппинг: позиция %d -> мотор %d\n", logicalPos, physicalMotor);
        }
      } else if (command.startsWith("set_inv:")) {
        int firstColon = command.indexOf(':', 8);
        int logicalPos = command.substring(8, firstColon).toInt();
        int value = command.substring(firstColon + 1).toInt();

        if (logicalPos >= 0 && logicalPos < 4) {
          motorInvert[logicalPos] = (value == 1);
          Serial.printf("Инверсия: позиция %d = %d\n", logicalPos, value);
        }
      }
    }
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    pCharacteristic->setValue(getConfigJSON().c_str());
  }
};

class TestMotorCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      String command = String(value.c_str());
      Serial.println("Test команда: " + command);

      // test_0_fwd, test_1_bwd, test_2_stop и т.д.
      if (command.startsWith("test_")) {
        int pos = command.substring(5, 6).toInt();
        String action = command.substring(7);

        if (pos >= 0 && pos < 4) {
          int logicalMotor = pos + 1;
          if (action == "fwd") {
            setMotor(logicalMotor, currentSpeed);
          } else if (action == "bwd") {
            setMotor(logicalMotor, -currentSpeed);
          } else if (action == "stop") {
            setMotor(logicalMotor, 0);
          }
        }
      }
    }
  }
};

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("   ESP32 Omni Robot Controller");
  Serial.println("   Web Bluetooth Edition");
  Serial.println("=================================\n");

  Serial.println("TA6586 управление:");
  Serial.println("  Вперёд: D0=HIGH/PWM, D1=LOW");
  Serial.println("  Назад:  D0=LOW/PWM (инверсный), D1=HIGH");
  Serial.println("  Холостой: D0=LOW, D1=LOW\n");

  // Загрузить конфигурацию
  loadConfig();

  // Настройка пинов моторов
  pinMode(MOTOR1_D1, OUTPUT);
  pinMode(MOTOR2_D1, OUTPUT);
  pinMode(MOTOR3_D1, OUTPUT);
  pinMode(MOTOR4_D1, OUTPUT);

  // PWM настройка
  ledcSetup(PWM_CHANNEL_M1, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CHANNEL_M2, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CHANNEL_M3, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CHANNEL_M4, PWM_FREQ, PWM_RES);

  ledcAttachPin(MOTOR1_D0, PWM_CHANNEL_M1);
  ledcAttachPin(MOTOR2_D0, PWM_CHANNEL_M2);
  ledcAttachPin(MOTOR3_D0, PWM_CHANNEL_M3);
  ledcAttachPin(MOTOR4_D0, PWM_CHANNEL_M4);

  stopAllMotors();
  Serial.println("✓ Моторы инициализированы");

  // BLE инициализация
  Serial.println("\nИнициализация BLE...");
  BLEDevice::init("Omni Robot");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Command характеристика
  pCharCommand = pService->createCharacteristic(
    CHAR_UUID_COMMAND,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharCommand->setCallbacks(new CommandCallbacks());

  // Joystick характеристика
  pCharJoystick = pService->createCharacteristic(
    CHAR_UUID_JOYSTICK,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharJoystick->setCallbacks(new JoystickCallbacks());

  // Speed характеристика
  pCharSpeed = pService->createCharacteristic(
    CHAR_UUID_SPEED,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharSpeed->setCallbacks(new SpeedCallbacks());

  // Config характеристика
  pCharConfig = pService->createCharacteristic(
    CHAR_UUID_CONFIG,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharConfig->setCallbacks(new ConfigCallbacks());
  pCharConfig->addDescriptor(new BLE2902());

  // Test Motor характеристика
  pCharTest = pService->createCharacteristic(
    CHAR_UUID_TEST,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharTest->setCallbacks(new TestMotorCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("✓ BLE сервер запущен");
  Serial.println("Устройство: Omni Robot");
  Serial.println("Ожидание подключения...\n");
  Serial.println("=================================\n");
}

// ==================== LOOP ====================

void loop() {
  delay(10);
}
