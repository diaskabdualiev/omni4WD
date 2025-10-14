#include <Arduino.h>
#include "ESP32Wiimote.h"
#include <Preferences.h>

// ==================== КОНФИГУРАЦИЯ ====================

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

ESP32Wiimote wiimote;
Preferences preferences;

// Текущая скорость (0-255)
int currentSpeed = 200;  // ~80% от 255

// Конфигурация моторов
// motorMapping[логическая_позиция] = физический_мотор
// Логические позиции: 0=передний-правый, 1=передний-левый, 2=задний-левый, 3=задний-правый
// По умолчанию для X-конфигурации: M1↗ M2↖ M3↙ M4↘
int motorMapping[4] = {1, 2, 3, 4};  // По умолчанию: прямое соответствие
bool motorInvert[4] = {false, false, false, false};  // Инверсия направления

// Состояние кнопок для детекции изменений
uint16_t lastButtonState = 0;

// Для контроля скорости изменения скорости
unsigned long lastSpeedChangeTime = 0;
const int SPEED_CHANGE_INTERVAL = 200;  // мс между изменениями
const int SPEED_STEP = 25;  // шаг изменения скорости

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

// ==================== ОБРАБОТКА WIIMOTE ====================

void handleWiimoteInput() {
  if (wiimote.available() > 0) {
    uint16_t button = wiimote.getButtonState();

    // Отладочный вывод при изменении состояния кнопок
    if (button != lastButtonState) {
      Serial.printf("Buttons: 0x%04x = ", (int)button);

      if (button & ESP32Wiimote::BUTTON_A)     Serial.print("A ");
      if (button & ESP32Wiimote::BUTTON_B)     Serial.print("B ");
      if (button & ESP32Wiimote::BUTTON_ONE)   Serial.print("1 ");
      if (button & ESP32Wiimote::BUTTON_TWO)   Serial.print("2 ");
      if (button & ESP32Wiimote::BUTTON_MINUS) Serial.print("- ");
      if (button & ESP32Wiimote::BUTTON_PLUS)  Serial.print("+ ");
      if (button & ESP32Wiimote::BUTTON_HOME)  Serial.print("HOME ");
      if (button & ESP32Wiimote::BUTTON_LEFT)  Serial.print("< ");
      if (button & ESP32Wiimote::BUTTON_RIGHT) Serial.print("> ");
      if (button & ESP32Wiimote::BUTTON_UP)    Serial.print("^ ");
      if (button & ESP32Wiimote::BUTTON_DOWN)  Serial.print("v ");

      Serial.printf("| Speed: %d\n", currentSpeed);
      lastButtonState = button;
    }

    // Приоритет: HOME (аварийная остановка) имеет наивысший приоритет
    if (button & ESP32Wiimote::BUTTON_HOME) {
      stopAllMotors();
      Serial.println("🛑 АВАРИЙНАЯ ОСТАНОВКА!");
      return;
    }

    // Управление скоростью кнопками +/-
    unsigned long currentTime = millis();
    if (currentTime - lastSpeedChangeTime > SPEED_CHANGE_INTERVAL) {
      if (button & ESP32Wiimote::BUTTON_PLUS) {
        currentSpeed = min(255, currentSpeed + SPEED_STEP);
        lastSpeedChangeTime = currentTime;
        Serial.printf("⚡ Скорость увеличена: %d\n", currentSpeed);
      } else if (button & ESP32Wiimote::BUTTON_MINUS) {
        currentSpeed = max(50, currentSpeed - SPEED_STEP);
        lastSpeedChangeTime = currentTime;
        Serial.printf("⚡ Скорость уменьшена: %d\n", currentSpeed);
      }
    }

    // Векторное сложение движений для комбинированного управления
    // Позволяет нажимать несколько кнопок одновременно (например, вперёд + стрейф)
    float motor1 = 0, motor2 = 0, motor3 = 0, motor4 = 0;

    // D-pad управление (для ГОРИЗОНТАЛЬНОГО положения Wiimote)
    // X-конфигурация: M1↗ M2↖ M3↙ M4↘

    // Вперёд/Назад
    if (button & ESP32Wiimote::BUTTON_LEFT) {
      // Вперёд: все моторы +1
      motor1 += 1.0;
      motor2 += 1.0;
      motor3 += 1.0;
      motor4 += 1.0;
    }
    if (button & ESP32Wiimote::BUTTON_RIGHT) {
      // Назад: все моторы -1
      motor1 -= 1.0;
      motor2 -= 1.0;
      motor3 -= 1.0;
      motor4 -= 1.0;
    }

    // Стрейф (D-pad UP/DOWN для горизонтального Wiimote)
    if (button & ESP32Wiimote::BUTTON_UP) {
      // Стрейф влево: M1-, M2+, M3+, M4-
      motor1 -= 1.0;
      motor2 += 1.0;
      motor3 += 1.0;
      motor4 -= 1.0;
    }
    if (button & ESP32Wiimote::BUTTON_DOWN) {
      // Стрейф вправо: M1+, M2-, M3-, M4+
      motor1 += 1.0;
      motor2 -= 1.0;
      motor3 -= 1.0;
      motor4 += 1.0;
    }

    // Поворот (кнопки A, B, 1, 2)
    if ((button & ESP32Wiimote::BUTTON_A) || (button & ESP32Wiimote::BUTTON_TWO)) {
      // Поворот вправо: M1+, M2-, M3+, M4-
      motor1 += 1.0;
      motor2 -= 1.0;
      motor3 += 1.0;
      motor4 -= 1.0;
    }
    if ((button & ESP32Wiimote::BUTTON_B) || (button & ESP32Wiimote::BUTTON_ONE)) {
      // Поворот влево: M1-, M2+, M3-, M4+
      motor1 -= 1.0;
      motor2 += 1.0;
      motor3 -= 1.0;
      motor4 += 1.0;
    }

    // Нормализация и применение скорости
    float maxVal = max(max(abs(motor1), abs(motor2)), max(abs(motor3), abs(motor4)));

    if (maxVal > 0.01) {
      // Есть движение - нормализуем и применяем currentSpeed
      float scale = currentSpeed / maxVal;
      setMotor(1, (int)(motor1 * scale));
      setMotor(2, (int)(motor2 * scale));
      setMotor(3, (int)(motor3 * scale));
      setMotor(4, (int)(motor4 * scale));
    } else {
      // Ни одна кнопка движения не нажата - остановка
      stopAllMotors();
    }
  }
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("   ESP32 Omni Robot Controller");
  Serial.println("   Nintendo Wii Remote Edition");
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

  // Инициализация Wiimote
  Serial.println("\nИнициализация Wii Remote...");
  wiimote.init();
  Serial.println("✓ Wiimote инициализирован");
  Serial.println("\n=================================");
  Serial.println("Нажми 1+2 на Wiimote для подключения");
  Serial.println("=================================\n");
  Serial.println("Управление (Wiimote в ГОРИЗОНТАЛЬНОМ положении):");
  Serial.println("  D-pad ←   = Вперёд");
  Serial.println("  D-pad →   = Назад");
  Serial.println("  D-pad ↑   = Стрейф влево");
  Serial.println("  D-pad ↓   = Стрейф вправо");
  Serial.println("  Кнопка A  = Поворот вправо");
  Serial.println("  Кнопка B  = Поворот влево");
  Serial.println("  Кнопка 1  = Поворот влево (дублирует B)");
  Serial.println("  Кнопка 2  = Поворот вправо (дублирует A)");
  Serial.println("  Кнопка +  = Увеличить скорость");
  Serial.println("  Кнопка -  = Уменьшить скорость");
  Serial.println("  Кнопка HOME = АВАРИЙНАЯ ОСТАНОВКА");
  Serial.println("\n✨ Можно нажимать несколько кнопок одновременно!");
  Serial.println("   Например: ← + A = движение по диагонали");
  Serial.printf("\n⚡ Текущая скорость: %d (диапазон: 50-255)\n", currentSpeed);
  Serial.println("\n=================================\n");
}

// ==================== LOOP ====================

void loop() {
  wiimote.task();
  handleWiimoteInput();
  delay(10);
}
