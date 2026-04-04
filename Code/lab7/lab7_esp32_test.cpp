/*
Hardware Tester
*/

// ----- 74hc595 (seven-segment display) -----
#define SSD_HC_DATA 13
#define SSD_HC_LATCH 12
#define SSD_HC_CLOCK 14

// ----- 74hc595 (led matrix 8x8) -----
#define LM_HC_DATA 25
#define LM_HC_LATCH 26
#define LM_HC_CLOCK 27

// ----- LEDs -----
#define LED_R 21
#define LED_B 22

// ----- Joystick -----
#define VRX 34
#define VRY 35
#define SW 32

// ----- my functions -----
bool isCommonAnodeMatrix = true; // Зміни на false, якщо матриця світиться "навиворіт"
int dotX = 3;
int dotY = 3;

// Відправка даних на семисегментник (просто тестовий патерн)
void write7Seg(byte data) {
  digitalWrite(SSD_HC_LATCH, LOW);
  shiftOut(SSD_HC_DATA, SSD_HC_CLOCK, MSBFIRST, data); // Units
  shiftOut(SSD_HC_DATA, SSD_HC_CLOCK, MSBFIRST, data); // Tens
  digitalWrite(SSD_HC_LATCH, HIGH);
}

// Вивід однієї точки на матрицю
void drawMatrixDot(int x, int y) {
  byte rowByte, colByte;
  
  if (isCommonAnodeMatrix) {
    rowByte = (1 << y);       // Високий рівень для анода
    colByte = ~(1 << x);      // Низький рівень для катода
  } else {
    rowByte = ~(1 << y);      
    colByte = (1 << x);       
  }

  digitalWrite(LM_HC_LATCH, LOW);
  shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, colByte);
  shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, rowByte);
  digitalWrite(LM_HC_LATCH, HIGH);
}

// Повне очищення матриці (щоб не було "привидів")
void clearMatrix() {
  byte rowByte = isCommonAnodeMatrix ? 0x00 : 0xFF;
  byte colByte = isCommonAnodeMatrix ? 0xFF : 0x00;
  
  digitalWrite(LM_HC_LATCH, LOW);
  shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, colByte);
  shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, rowByte);
  digitalWrite(LM_HC_LATCH, HIGH);
}

// ----- task functions -----

// Тест 1: Блимання діодами
void taskTestLEDs() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_B, LOW);
  delay(500);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_B, HIGH);
  delay(500);
}

// Тест 2: Засвітити всі сегменти на екрані (почергово)
void taskTest7Seg() {
  write7Seg(0b01010101);
  delay(500);
  write7Seg(0b10101010);
  delay(500);
}

// Тест 3: Проста перевірка матриці (діагональ)
void taskTestMatrix() {
  for (int i = 0; i < 8; i++) {
    drawMatrixDot(i, i);
    delay(2); // Швидке оновлення для ілюзії статичного зображення
  }
}

// Тест 4: Джойстик + Матриця + Діоди (те що ти просив)
void taskInteractive() {
  static unsigned long lastMove = 0;
  
  // 1. Опитування джойстика кожні 150 мс (щоб точка не літала занадто швидко)
  if (millis() - lastMove > 150) {
    int vx = analogRead(VRX) - 2048; // Центр приблизно 2048
    int vy = analogRead(VRY) - 2048;

    if (vx > 600) dotX = (dotX + 1) % 8;
    if (vx < -600) dotX = (dotX - 1 + 8) % 8;
    if (vy > 600) dotY = (dotY + 1) % 8;
    if (vy < -600) dotY = (dotY - 1 + 8) % 8;
    
    lastMove = millis();
  }

  // 2. Опитування кнопки джойстика
  bool btnPressed = (digitalRead(SW) == LOW); // LOW означає натиснуто (через PULLUP)
  digitalWrite(LED_R, btnPressed ? HIGH : LOW);
  digitalWrite(LED_B, btnPressed ? HIGH : LOW);

  // 3. Відмальовка точки на матриці (постійна)
  drawMatrixDot(dotX, dotY);
  delay(2); // Невеличка затримка для стабільності
  clearMatrix(); // Очищення перед наступним циклом
}

// ----- core functions -----
void setup() {
  Serial.begin(115200);

  // Ініціалізація 74hc595
  pinMode(SSD_HC_DATA, OUTPUT); pinMode(SSD_HC_LATCH, OUTPUT); pinMode(SSD_HC_CLOCK, OUTPUT);
  pinMode(LM_HC_DATA, OUTPUT);  pinMode(LM_HC_LATCH, OUTPUT);  pinMode(LM_HC_CLOCK, OUTPUT);

  // Ініціалізація діодів
  pinMode(LED_R, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // Ініціалізація джойстика
  pinMode(SW, INPUT_PULLUP);
  analogReadResolution(12); // Гарантуємо 12-бітне читання (0-4095)
}

void loop() {
  // РОЗКОМЕНТУЙ ОДНУ ТАСКУ ДЛЯ ТЕСТУВАННЯ:
  
  // taskTestLEDs();
  // taskTest7Seg();
  // taskTestMatrix();
  taskInteractive(); 
}