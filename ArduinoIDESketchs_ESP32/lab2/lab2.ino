#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LED pins (безпечні GPIO)
const int greenPin  = 18;
const int yellowPin = 19;
const int redPin    = 23;

// Piezo pin
const int piezoPin = 27;

// HC-SR04 pins
const int trigPin = 26;
const int echoPin = 25;

// LCD: I2C address (може бути 0x27 або 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

float dist = 0;

long readUltrasonicDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH);
  return duration;
}

void setup() {
  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(piezoPin, OUTPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Distance:");
}

void loop() {
  dist = 0.01723 * readUltrasonicDistance(trigPin, echoPin); // cm

  // Вивід на LCD
  lcd.setCursor(0, 1);
  lcd.print("        "); // очищаємо попереднє значення
  lcd.setCursor(0, 1);
  lcd.print(dist);
  lcd.print(" cm");

  // LED + Piezo
  if (dist > 200) {
    digitalWrite(greenPin, HIGH);
    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, LOW);
    tone(piezoPin, 200, 200);
  } else if (dist > 150) {
    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, HIGH);
    digitalWrite(redPin, LOW);
    tone(piezoPin, 300, 200);
  } else if (dist > 100) {
    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, HIGH);
    tone(piezoPin, 400, 200);
  } else {
    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, HIGH);
    tone(piezoPin, 523, 200);
  }

  delay(200); // невелика затримка для стабільності
}