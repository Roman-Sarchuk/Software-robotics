#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ----- LED pins -----
const int greenPin  = 16;
const int yellowPin = 17;
const int redPin    = 18;

// ----- LCD (I2C) -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ----- Piezo pin -----
const int piezoPin = 27;

// ----- Servo -----
Servo servo;

// ----- Matrix Keypad -----
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
const int rowPins[ROWS] = {32, 33, 25, 14};
const int colPins[COLS] = {12, 13, 15, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ----- params -----
#define MAX_PASSWORD_LENGTH 4
#define STRING_TERMINATOR '\0'
#define APARTMENT_NUM "408"
#define PASSWORD "1234"

char passText[] = "Enter pass: ";
int passTextLen;
bool isPassTextDispalyed = false;
bool isOpenedTextDisplayed = false;

struct Entry {
  char password[MAX_PASSWORD_LENGTH + 1] = "";
  byte passIndex = 0;
  bool isOpened = false;
} entry;

// melody
struct Melody
{
  int frequencies[];
  int durations[];
  int size;

  Melody(int frequencies[], durations[]) {
    this.frequencies = frequencies;
    this.durations = durations;
    this.size = sizeof(frequencies) / sizeof(int)
  }
}

// Admitted melody
const Melody admittedMelody = new Melody(
  { 262, 262, 0, 262, 0, 196, 262, 330 },
  { 125, 125, 125, 125, 125, 125, 125, 125 }
);
// Rejected melody
const Melody rejectedMelody = new Melody(
  { 196, 196, 196, 147 },
  { 300, 300, 300, 600 }
);

// ----- My functions -----
void playMelody(Melody melody) {
  for (int i = 0; i < melody.size; i++)
  {
    tone(piezoPin, melody.frequencies[i], melody.durations[i]);
  } 
}

void processKey(char key) {
  if (!entry.isOpened && key == 'A') {
    checkAccess();
  } 
  else if (!entry.isOpened && key == 'D') {
    deleteEntry();
  }
  else if (!entry.isOpened && key == 'C') {
    clearEntry();
  }
  else if (entry.isOpened && key == 'B') {
    blockDoor();
  }
  else {
    processPassEntry(key);
  }
}

void processPassEntry(char value) {
  if (entry.passIndex < MAX_PASSWORD_LENGTH) {
    entry.password[entry.passIndex++] = value;
    entry.password[entry.passIndex] = STRING_TERMINATOR;
    lcd.setCursor(passTextLen + entry.passIndex, 1);
    lcd.print('*');
  }
}

void deleteEntry() {
  if (entry.passIndex > 0) {
    entry.password[entry.passIndex--] = '';
    entry.password[entry.passIndex] = STRING_TERMINATOR;
    lcd.setCursor(passTextLen + entry.passIndex, 1);
    lcd.print(' ');
  }
}

void clearEntry() {
  memset(entry.password, 0, sizeof(entry.password));
  entry.passIndex = 0;
  lcd.setCursor(5 + entry.passIndex, 1);
  for (int i = 0; i < MAX_PASSWORD_LENTH; i++) {
    lcd.print(' ');
  }
}

void checkAccess() {  // A
  if (strcmp(entry.password, PASSWORD) == 0) {
    unlockDoor();
  } else {
    denyAccess();
  }
}

void unlockDoor() {
  // admitted melody
  for (int i = 0; i < admittedMelody.size; i++) {
    tone(BUZZER, admittedMelody.frequencies);
    delay(admittedMelody.durations);
    noTone(BUZZER);
  }

  servo.write(0); // open lock

  digitalWrite(LED_GREEN, HIGH);

  entry.isOpened = true;

  clearLCD(1);
  lcd.setCursor(0, 1);
  lcd.print("ACCESS GRANTED");
  delay(1000);
}

void denyAccess() {
  // rejected melody
  for (int i = 0; i < rejectedMelody.size; i++) {
    tone(BUZZER, rejectedMelody.frequencies);
    delay(rejectedMelody.durations);
    noTone(BUZZER);
  }

  clearLCD(1);
  lcd.setCursor(0, 1);
  lcd.print("ACCESS DENIED");
  delay(1000);
}

void blockDoor() {
  servo.write(90);

  clearEntry();

  clearLCD(1);

  entry.isOpened = false;
}

void welcomeMessage() {
  lcd.setCursor(3, 0);
  lcd.print("WELCOME TO");
  lcd.setCursor(0, 1);
  lcd.print("DOOR LOCK SYSTEM");
  delay(3000);
  lcd.clear();
}

void clearLCD(int rowIndex) {
  lcd.setCursor(0, rowIndex);
  lcd.print("                ");
}

// ----- Core function -----
void setup() {
  Serial.begin(115200);
  
  // LEDs
  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin, OUTPUT);

  // Piezo
  pinMode(piezoPin, OUTPUT);

  // Sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // LCD
  lcd.init();
  lcd.backlight();

  // Servo
  servo.attach(26);
  servo.write(90);

  // other
  lcd.clear();
  welcomeMessage();

  lcd.setCursor(0, 0);
  lcd.print("Apartment #" + APARTMENT_NUM);

  passTextLen = sizeof(passText) / sizeof(char);
}

void loop() {
  if (!entry.isOpened && !isPassTextDispalyed)
  {
    clearLCD(1);
    lcd.setCursor(0, 1);
    lcd.print(passText);

    isPassTextDispalyed = true;
  }
  if (entry.isOpened && !isOpenedTextDisplayed) {
    clearLCD(1);
    lcd.setCursor(0, 1);
    lcd.print("Door is opened!");
  }

  char key = keypad.getKey();
  if (key != NO_KEY) {
    processKey(key);
  }
}