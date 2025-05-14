#include <SPI.h>     // Необходима для связи с RFID-RC522
#include <MFRC522.h> // Библиотека для RFID-RC522
#include <Servo.h>   // Библиотека для управления сервоприводом

// --- Конфигурация пинов ---
const int PIN_RED_LED = 2;
const int PIN_GREEN_LED = 3;
const int PIN_SERVO = 9;

const int PIN_RFID_SS = 10;
const int PIN_RFID_RST = 8;

// --- Конфигурация компонентов ---
// Инициализация RFID-модуля
MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);

// Инициализация сервопривода
Servo safeServo;


const int SERVO_ANGLE_CLOSED = 0;
const int SERVO_ANGLE_OPEN = 90;

// Длительность, на которую сейф остается открытым
const unsigned long OPEN_DURATION_MS = 5000;

// --- NFC-метка ---

const byte NUM_CARDS = 3;     // Количество разрешенных карточек
const byte UID_SIZE = 4;

byte correctUids[3][4] = {
  {0x85, 0xCE, 0xDB, 0xD1}, // <-- UID Карточки 1 (Домофон)
  {0xB7, 0x40, 0x94, 0x5B}, // <-- UID Карточки 2 (Агарков)
  {0x09, 0x10, 0x11, 0x12}  // <-- UID Карточки 3 (Сержанов)
};


// --- Состояния сейфа и переменные ---
enum SafeState { STATE_CLOSED, STATE_OPEN };
SafeState currentSafeState = STATE_CLOSED;

unsigned long timeSafeOpened = 0;


// Обновление состояния светодиодов и сервопривода в зависимости от статуса сейфа
void updateSafeHardware() {
  if (currentSafeState == STATE_CLOSED) {
    digitalWrite(PIN_RED_LED, HIGH);
    digitalWrite(PIN_GREEN_LED, LOW);
    safeServo.write(SERVO_ANGLE_CLOSED);
  } else {
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_GREEN_LED, HIGH);
    safeServo.write(SERVO_ANGLE_OPEN);
  }
}

// Проверка, соответствует ли текущая считанная NFC-метка любой из разрешенных
bool checkNFCTag(MFRC522::Uid *uid) {
  if (uid->size != UID_SIZE) {
    Serial.print("Scanned UID size mismatch: ");
    Serial.println(uid->size);
    return false;
  }

  // Итерируем по списку разрешенных UID
  for (int i = 0; i < NUM_CARDS; i++) {
    bool match = true;
    for (byte j = 0; j < UID_SIZE; j++) {
      if (uid->uidByte[j] != correctUids[i][j]) {
        match = false;
        break;
      }
    }

    if (match) {
      Serial.print("Match found with correct UID index: ");
      Serial.println(i);
      return true;
    }
  }

  Serial.println("No match found in correct UIDs list.");
  return false;
}


void setup() {
  // Настройка пинов светодиодов
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);

  Serial.begin(9600);
  while (!Serial);
  Serial.println("Simple Safe System Starting...");
// Инициализация SPI и RFID-модуля
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("RFID-RC522 initialized.");

  safeServo.attach(PIN_SERVO);

  currentSafeState = STATE_CLOSED;
  updateSafeHardware();

  Serial.println("Setup complete. Safe is closed.");
  Serial.println("Scan an NFC tag to open.");
}

void loop() {
  unsigned long currentTime = millis();

  // --- Обработка NFC-метки (только если сейф закрыт) ---
  if (currentSafeState == STATE_CLOSED && mfrc522.PICC_IsNewCardPresent()) {
    // Считываем UID метки
    if (mfrc522.PICC_ReadCardSerial()) {
      Serial.print("NFC Tag Scanned. UID:");
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      }
      Serial.println();

      // Проверяем, является ли считанная метка правильной
      if (checkNFCTag(&mfrc522.uid)) {
        Serial.println("NFC Tag CORRECT! Opening safe...");
        currentSafeState = STATE_OPEN;
        timeSafeOpened = currentTime;
        updateSafeHardware();
      } else {
        Serial.println("NFC Tag INCORRECT.");
      }

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }

  // --- Проверка времени закрытия (если сейф открыт) ---
  if (currentSafeState == STATE_OPEN) {
    if (currentTime - timeSafeOpened >= OPEN_DURATION_MS) {
      Serial.println("Open duration passed. Closing safe...");
      currentSafeState = STATE_CLOSED;
      updateSafeHardware();
    }
  }

  // Небольшая задержка для стабильной работы
  delay(50);
}
