/*

Components:
- freenove esp32 wrover
- 2 LEDs (red, green)
- 2 resistors (220 ohm)
- LCD 16x2 with I2C module
- Piezo buzzer
- Servo motor
- Keypad 4x4

*/

#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

#define MAX_PASSWORD_LENGTH 6
#define MAX_APARTMENT_LENGTH 3
#define STRING_TERMINATOR '\0'

#define BUZZER 27
#define LED_GREEN 18
#define LED_RED 19

#define APARTMENT "19"
#define PASSWORD "1234"

Servo servo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Матрична клавіатура 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
  // {'D', 'C', 'B', 'A'},
  // {'#', '9', '6', '3'},
  // {'0', '8', '5', '2'},
  // {'*', '7', '4', '1'}
};

// Піни рядів та стовпців для ESP32
byte rowPins[ROWS] = {32, 33, 25, 14};
byte colPins[COLS] = {12, 13, 15, 2};
// byte rowPins[ROWS] = {14, 25, 33, 32};
// byte colPins[COLS] = {2, 15, 13, 12};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

struct Entry {
  char apartment[MAX_APARTMENT_LENGTH + 1] = "";
  char password[MAX_PASSWORD_LENGTH + 1] = "";
  byte aptIndex = 0;
  byte passIndex = 0;
  bool enteringApt = true;
  bool apartmentConfirmed = false;
} entry;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  servo.attach(26);
  servo.write(90); // закритий замок

  lcd.init();
  lcd.backlight();
  welcomeMessage();
}

void loop() {
  lcd.setCursor(0, 0);
  lcd.print(entry.enteringApt ? "ENTER APARTMENT: " : "ENTER PASSWORD: ");

  char key = keypad.getKey();
  if (key != NO_KEY) {
    processKey(key);
  }
}

void processKey(char key) {
  if (key == 'C') {
    resetEntry();
  } else if (key == 'D') {
    checkAccess();
  } else {
    if (entry.enteringApt) {
      if (key == '#' && entry.aptIndex > 0) {
        entry.enteringApt = false;
        entry.apartmentConfirmed = true;
        lcd.clear();
      } else if (entry.aptIndex < MAX_APARTMENT_LENGTH && key != '#') {
        entry.apartment[entry.aptIndex++] = key;
        entry.apartment[entry.aptIndex] = STRING_TERMINATOR;
        lcd.setCursor(6 + entry.aptIndex, 1);
        lcd.print(key);
      }
    } else {
      if (entry.passIndex < MAX_PASSWORD_LENGTH) {
        entry.password[entry.passIndex++] = key;
        entry.password[entry.passIndex] = STRING_TERMINATOR;
        lcd.setCursor(5 + entry.passIndex, 1);
        lcd.print('*');
      }
    }
  }
}

void checkAccess() {
  if (strcmp(entry.apartment, APARTMENT) == 0 && strcmp(entry.password, PASSWORD) == 0) {
    unlockDoor();
  } else {
    denyAccess();
  }
  resetEntry();
}

void unlockDoor() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER, 300);
    delay(200);
    noTone(BUZZER);
    delay(200);
  }
  servo.write(0); // відкритий замок
  lcd.clear();
  digitalWrite(LED_GREEN, HIGH);
  lcd.setCursor(0, 0);
  lcd.print("ACCESS GRANTED");
  lcd.setCursor(0, 1);
  lcd.print("DOOR OPENED");
  delay(2000);
  digitalWrite(LED_GREEN, LOW);
  servo.write(90); // закритий замок
  lcd.clear();
}

void denyAccess() {
  tone(BUZZER, 500);
  lcd.clear();
  digitalWrite(LED_RED, HIGH);
  lcd.setCursor(0, 0);
  lcd.print("ACCESS DENIED!");
  lcd.setCursor(0, 1);
  lcd.print("TRY AGAIN");
  delay(2000);
  digitalWrite(LED_RED, LOW);
  lcd.clear();
  noTone(BUZZER);
}

void resetEntry() {
  memset(entry.apartment, 0, sizeof(entry.apartment));
  memset(entry.password, 0, sizeof(entry.password));
  entry.aptIndex = 0;
  entry.passIndex = 0;
  entry.enteringApt = true;
  entry.apartmentConfirmed = false;
  lcd.clear();
}

void welcomeMessage() {
  lcd.setCursor(3, 0);
  lcd.print("WELCOME TO");
  lcd.setCursor(0, 1);
  lcd.print("DOOR LOCK SYSTEM");
  delay(3000);
  lcd.clear();
}