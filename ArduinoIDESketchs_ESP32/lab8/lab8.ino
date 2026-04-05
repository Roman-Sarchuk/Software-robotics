/*

Components:
- freenove esp32 wrover
- HC-SR04 x2
- 2 resistors (1 kOhm)
- 7-segment display (1 digit) x2
- 14 resistors (220 ohm)
- RGB LED
- 3 resistors (220 ohm)
- Piezo buzzer
- Servo motor

*/

#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

#define MAX_PASSWORD_LENGTH 6
#define MAX_APARTMENT_LENGTH 3
#define STRING_TERMINATOR '\0'
#define buzzer 4
#define LED_GREEN 2
#define LED_RED 3

Servo servo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'D', 'C', 'B', 'A'},
  {'#', '9', '6', '3'},
  {'0', '8', '5', '2'},
  {'*', '7', '4', '1'}
};
byte rowPins[ROWS] = {6,7,8,9};
byte colPins[COLS] = {10,11,12,13};
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
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  servo.attach(5);
  servo.write(90);
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
  if (strcmp(entry.apartment, "101") == 0 && strcmp(entry.password, "1234") == 0) {
    unlockDoor();
  } else {
    denyAccess();
  }
  resetEntry();
}

void unlockDoor() {
    for (int i = 0; i < 3; i++) {
    tone(11, 300);
    delay(200);
    noTone(11);
    delay(200);
  }
  servo.write(0);
  lcd.clear();
  lcd.setCursor(0, 0);
  digitalWrite(LED_GREEN, HIGH);
  lcd.print("ACCESS GRANTED");
  lcd.setCursor(0, 1);
  lcd.print("DOOR OPENED");
  delay(2000);
  digitalWrite(LED_GREEN, LOW);
  servo.write(90);
  lcd.clear();
}

void denyAccess() {
  tone(11, 500);
  lcd.clear();
  lcd.setCursor(0, 0);
  digitalWrite(LED_RED, HIGH);
  lcd.print("ACCESS DENIED!");
  lcd.setCursor(0, 1);
  lcd.print("TRY AGAIN");
  delay(2000);
  digitalWrite(LED_RED, LOW);
  lcd.clear();
  noTone(11);
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