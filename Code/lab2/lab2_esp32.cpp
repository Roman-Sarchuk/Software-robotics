#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ----- LED pins -----
const int greenPin  = 16;
const int yellowPin = 17;
const int redPin    = 18;

// ----- LCD (I2C) -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ----- Piezo pin -----
const int piezoPin = 27;

// ----- HC-SR04 pins -----
const int trigPin = 26;
const int echoPin = 25;

// ----- Buttons -----
const int powerButton = 33;

// ----- params -----
enum SoundType 
{
  GREEN,
  YELLOW,
  RED,
  CRITICAL
};

volatile bool isSystemEnabled = true;
unsigned long lastInterrupt = 0;

bool isDistanceTextDisplayed = false;
bool isSystemOffTextDisplayed = false;

float dist = 0;

// ----- My functions -----
long readUltrasonicDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH);
  return duration;
}

void makeSound(SoundType soundType) {
  switch (soundType)
  {
    case GREEN:
      tone(piezoPin, 150, 200);
      break;

    case YELLOW:
      tone(piezoPin, 300, 200);
      break;

    case RED:
      tone(piezoPin, 400, 200);
      break;

    case CRITICAL:
      tone(piezoPin, 523, 200);
      break;
  }
}

void clearLCD(int rowIndex) {
  lcd.setCursor(0, rowIndex);
  lcd.print("                ");
}


// ----- intrerrapt function -----
void IRAM_ATTR handleButtonInterrupt()
{
  unsigned long now = millis();

  if (now - lastInterrupt > 200)
  {
    isSystemEnabled = !isSystemEnabled;
    lastInterrupt = now;
  }
}

// ----- Core functions -----
void setup() {
  // Pins
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

  // Interrupt
  pinMode(powerButton, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(powerButton),
    handleButtonInterrupt,
    FALLING
  );
}

void loop() {
  if (isSystemEnabled)
  {
    // get value
    dist = 0.01723 * readUltrasonicDistance(trigPin, echoPin); // cm

    // processed LCD
    if (isSystemOffTextDisplayed)
    {
      clearLCD(0);
      isSystemOffTextDisplayed = false;
    }
    if (!isDistanceTextDisplayed)
    {
      lcd.setCursor(0, 0);
      lcd.print("Distance:");
      isDistanceTextDisplayed = true;
    }
    clearLCD(1);
    lcd.setCursor(0, 1);
    lcd.print(dist);
    lcd.print(" cm");

    // LED + Piezo
    if (dist > 300) {
      digitalWrite(greenPin, HIGH);
      digitalWrite(yellowPin, LOW);
      digitalWrite(redPin, LOW);
      makeSound(GREEN);
      
    } else if (dist > 200) {
      digitalWrite(greenPin, LOW);
      digitalWrite(yellowPin, HIGH);
      digitalWrite(redPin, LOW);
      makeSound(YELLOW);
    } else if (dist > 100) {
      digitalWrite(greenPin, LOW);
      digitalWrite(yellowPin, LOW);
      digitalWrite(redPin, HIGH);
      makeSound(RED);
    } else {
      digitalWrite(greenPin, LOW);
      digitalWrite(yellowPin, LOW);
      digitalWrite(redPin, HIGH);
      makeSound(CRITICAL);
    }

  }
  else {
    if (isDistanceTextDisplayed)
    {
      clearLCD(0);
      clearLCD(1);
      isDistanceTextDisplayed = false;
    }
    if (!isSystemOffTextDisplayed)
    {
      lcd.setCursor(0, 0);
      lcd.print("System OFF");
      isSystemOffTextDisplayed = true;
    }
  }
  delay(200);
}