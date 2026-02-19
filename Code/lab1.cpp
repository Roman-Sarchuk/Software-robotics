#include <LiquidCrystal_I2C.h>

const int greenPin = 4;
const int yellowPin = 3;
const int redPin = 2;

LiquidCrystal_I2C lcd_1(0x20, 16, 2);


void setup() {
  lcd_1.init();
  lcd_1.backlight();

  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin, OUTPUT);
}


void countdown(int pin, String colorText, int seconds, bool isTimer = false) {
  digitalWrite(greenPin, LOW);
  digitalWrite(yellowPin, LOW);
  digitalWrite(redPin, LOW);
  digitalWrite(pin, HIGH);

  if (isTimer) {
  	for(int i = seconds; i > 0; i--) {
      lcd_1.clear();
      lcd_1.setCursor(0, 0);
      lcd_1.print(colorText + ": " + i + "s");
      delay(1000);
    }
  }
  else {
  	lcd_1.clear();
    lcd_1.setCursor(0, 0);
    lcd_1.print(colorText);
    delay(1000*seconds);
  }
}

void loop() {
  countdown(redPin, "Red", 5, true);

  countdown(yellowPin, "Yellow is ON", 2);

  countdown(greenPin, "Green", 5, true);

  countdown(yellowPin, "Yellow is ON", 2);
}
