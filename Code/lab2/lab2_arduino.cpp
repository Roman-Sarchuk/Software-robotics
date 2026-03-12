#include <LiquidCrystal_I2C.h>

const int greenPin = 4;
const int yellowPin = 3;
const int redPin = 2;
const int piezoPin = 5;
const int distanceSensorPin = 6;

float dist = 0;

LiquidCrystal_I2C lcd_1(32, 16, 2);

long readUltrasonicDistance(int pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(2);

  digitalWrite(pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(pin, LOW);

  pinMode(pin, INPUT);
  return pulseIn(pin, HIGH);
}


void setup()
{
  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(piezoPin, OUTPUT);

  lcd_1.init();
  lcd_1.setCursor(0, 0);
  lcd_1.backlight();
  lcd_1.display();
}

void loop()
{
  dist = 0.01723 * readUltrasonicDistance(distanceSensorPin);

  lcd_1.setCursor(0, 0);
  lcd_1.print(dist);
  lcd_1.setCursor(0, 0);
  lcd_1.setCursor(7, 0);
  lcd_1.print("cm");
  
  if (dist > 200) {
    digitalWrite(greenPin, HIGH);
    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, LOW);
    tone(piezoPin, 200, 2000); // play tone 60 (C5 = 523 Hz)
  }
  else if (dist > 150 && dist < 200) {
    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, HIGH);
    digitalWrite(redPin, LOW);
    tone(piezoPin, 300, 2000); // play tone 60 (C5 = 523 Hz)
  }
  else if (dist > 100 && dist < 150) {
    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, HIGH);
    tone(piezoPin, 400, 2000); // play tone 60 (C5 = 523 Hz)
  }
  else {
    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, HIGH);
    tone(piezoPin, 523, 2000); // play tone 60 (C5 = 523 Hz)
  }

  delay(10); // Delay a little bit to improve simulation performance
}