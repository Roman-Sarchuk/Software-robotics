#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ----- LED pins (безпечні GPIO) -----
const int greenPin  = 18;
const int yellowPin = 19;
const int redPin    = 23;

// ----- LCD (адреса 0x27 найчастіша у Wokwi) -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Wire.begin(21, 22); // SDA, SCL для ESP32
  
  lcd.init();
  lcd.backlight();

  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin, OUTPUT);
}

void countdown(int pin, String text, int seconds, bool timerMode = false) {
  digitalWrite(greenPin, LOW);
  digitalWrite(yellowPin, LOW);
  digitalWrite(redPin, LOW);
  digitalWrite(pin, HIGH);

  if (timerMode) {
    for (int i = seconds; i > 0; i--) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(text);
      lcd.setCursor(0, 1);
      lcd.print("Time: ");
      lcd.print(i);
      lcd.print(" s");
      delay(1000);
    }
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(text);
    delay(seconds * 1000);
  }
}

void loop() {
  countdown(redPin, "RED", 5, true);
  countdown(yellowPin, "YELLOW", 2);
  countdown(greenPin, "GREEN", 5, true);
  countdown(yellowPin, "YELLOW", 2);
}