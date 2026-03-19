/*
  ============================================================
  Snake Game — Freenove ESP32-WROVER
  ============================================================

  HARDWARE:
  ─────────────────────────────────────────────────────────
  7-SEGMENT DISPLAY (2 digits via 2x 74HC595):
    LATCH  → GPIO 18
    DATA   → GPIO 19
    CLK    → GPIO 5

  LED MATRIX 8x8 (via 2x 74HC595, daisy-chained):
    LATCH  → GPIO 26
    DATA   → GPIO 27
    CLK    → GPIO 25

  JOYSTICK:
    VRX    → GPIO 32 (ADC)
    VRY    → GPIO 33 (ADC)
    SW     → GPIO 15 (INPUT_PULLUP, active LOW)

  GAME OVER LED (червона):
    LED+   → GPIO 2 (через резистор ~220Ω)

  ─────────────────────────────────────────────────────────
  МАТРИЦЯ — полярність:
    Якщо діоди не світяться — зміни MATRIX_ANODE_ROW на false
  ─────────────────────────────────────────────────────────
*/

// ============================================================
//  НАЛАШТУВАННЯ — змінюй тут
// ============================================================

// true  = рядки (ROW) — аноди, стовпці (COL) — катоди (анодна матриця)
// false = рядки (ROW) — катоди, стовпці (COL) — аноди (катодна матриця)
#define MATRIX_ANODE_ROW true

// Піни 7-сегментних дисплеїв
#define SSD_LATCH  18
#define SSD_DATA   19
#define SSD_CLK    5

// Піни LED матриці
#define MTX_LATCH  26
#define MTX_DATA   27
#define MTX_CLK    25

// Піни джойстика
#define JOY_X      32
#define JOY_Y      33
#define JOY_BTN    15

// Пін червоного LED (game over)
#define LED_RED    2

// Гравець і яблуко — не чіпай якщо не знаєш
#define GRID_SIZE  8
#define MAX_SNAKE  64

// Швидкість гри (мс між кроками)
#define GAME_SPEED_MS  300

// Поріг ADC для джойстика (0–4095, центр ~2048)
#define JOY_THRESHOLD  800

// Час мигання яблука (мс)
#define APPLE_BLINK_MS 400

// ============================================================
//  ТИПИ
// ============================================================

enum Direction { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
enum GameState { STATE_PAUSE, STATE_RUNNING, STATE_GAMEOVER, STATE_WIN };

struct Point {
  int8_t x, y;
};

// ============================================================
//  ГЛОБАЛЬНІ ЗМІННІ
// ============================================================

Point snake[MAX_SNAKE];
uint8_t snakeLen = 0;
Direction dir = DIR_RIGHT;
Direction nextDir = DIR_RIGHT;
Point apple;
uint8_t score = 0;
GameState gameState = STATE_PAUSE;

unsigned long lastStep = 0;
unsigned long lastBlink = 0;
bool appleVisible = true;

// Дебаунс кнопки джойстика
unsigned long lastBtnPress = 0;
const unsigned long BTN_DEBOUNCE = 300;

// ============================================================
//  7-СЕГМЕНТНИЙ ДИСПЛЕЙ
// ============================================================

const uint8_t digitTable[] = {
  0b11000000, // 0
  0b11111001, // 1
  0b10100100, // 2
  0b10110000, // 3
  0b10011001, // 4
  0b10010010, // 5
  0b10000010, // 6
  0b11111000, // 7
  0b10000000, // 8
  0b10010000, // 9
};
const uint8_t SEG_BLANK = 0xFF;
const uint8_t SEG_DASH  = 0b10111111;

void sendScore(uint8_t high, uint8_t low) {
  digitalWrite(SSD_LATCH, LOW);
  shiftOut(SSD_DATA, SSD_CLK, MSBFIRST, low);
  shiftOut(SSD_DATA, SSD_CLK, MSBFIRST, high);
  digitalWrite(SSD_LATCH, HIGH);
}

void displayScore() {
  uint8_t hi = score / 10;
  uint8_t lo = score % 10;
  sendScore(hi ? digitTable[hi] : SEG_BLANK, digitTable[lo]);
}

void displayDash() {
  sendScore(SEG_DASH, SEG_DASH);
}

// ============================================================
//  LED МАТРИЦЯ — мультиплексування
// ============================================================

// Буфер матриці: frameBuffer[row] = bitmask стовпців (біт7=col0)
uint8_t frameBuffer[GRID_SIZE] = {0};

// Надсилає один рядок у матрицю через 2 shift registers
// reg1 = дані рядка (ROW select), reg2 = дані стовпців (COL data)
void sendMatrixRow(uint8_t rowSelect, uint8_t colData) {
  digitalWrite(MTX_LATCH, LOW);
#if MATRIX_ANODE_ROW
  // Анодна матриця: рядок HIGH = активний, стовпець LOW = світиться
  shiftOut(MTX_DATA, MTX_CLK, MSBFIRST, ~colData); // COL — катоди, інвертуємо
  shiftOut(MTX_DATA, MTX_CLK, MSBFIRST, rowSelect); // ROW — аноди
#else
  // Катодна матриця: рядок LOW = активний, стовпець HIGH = світиться
  shiftOut(MTX_DATA, MTX_CLK, MSBFIRST, colData);
  shiftOut(MTX_DATA, MTX_CLK, MSBFIRST, ~rowSelect);
#endif
  digitalWrite(MTX_LATCH, HIGH);
}

// Відображає один кадр (викликати в loop якомога частіше)
void refreshMatrix() {
  for (uint8_t row = 0; row < GRID_SIZE; row++) {
    uint8_t rowBit = (1 << (7 - row));
    sendMatrixRow(rowBit, frameBuffer[row]);
    delayMicroseconds(600); // час засвічення одного рядка
    sendMatrixRow(0, 0);    // гасимо перед наступним рядком (ghosting)
  }
}

void clearFrame() {
  memset(frameBuffer, 0, sizeof(frameBuffer));
}

void setPixel(int8_t x, int8_t y, bool on) {
  if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return;
  if (on)
    frameBuffer[y] |= (1 << (7 - x));
  else
    frameBuffer[y] &= ~(1 << (7 - x));
}

// ============================================================
//  ІГРОВА ЛОГІКА
// ============================================================

void spawnApple() {
  // Збираємо вільні клітинки
  Point free[MAX_SNAKE];
  uint8_t freeCount = 0;
  for (int8_t y = 0; y < GRID_SIZE; y++) {
    for (int8_t x = 0; x < GRID_SIZE; x++) {
      bool occupied = false;
      for (uint8_t i = 0; i < snakeLen; i++) {
        if (snake[i].x == x && snake[i].y == y) { occupied = true; break; }
      }
      if (!occupied) { free[freeCount++] = {x, y}; }
    }
  }
  if (freeCount == 0) { gameState = STATE_WIN; return; }
  uint8_t idx = random(freeCount);
  apple = free[idx];
  appleVisible = true;
  lastBlink = millis();
}

void initGame() {
  snakeLen = 3;
  snake[0] = {4, 3}; // голова
  snake[1] = {3, 3};
  snake[2] = {2, 3};
  dir = DIR_RIGHT;
  nextDir = DIR_RIGHT;
  score = 0;
  gameState = STATE_PAUSE;
  digitalWrite(LED_RED, LOW);
  spawnApple();
  displayScore();
}

void gameStep() {
  // Застосовуємо новий напрямок (не можна розвернутись)
  if (!((nextDir == DIR_UP    && dir == DIR_DOWN)  ||
        (nextDir == DIR_DOWN  && dir == DIR_UP)    ||
        (nextDir == DIR_LEFT  && dir == DIR_RIGHT) ||
        (nextDir == DIR_RIGHT && dir == DIR_LEFT))) {
    dir = nextDir;
  }

  // Нова голова
  Point head = snake[0];
  switch (dir) {
    case DIR_UP:    head.y--; break;
    case DIR_DOWN:  head.y++; break;
    case DIR_LEFT:  head.x--; break;
    case DIR_RIGHT: head.x++; break;
  }

  // Перенесення через стінки
  head.x = (head.x + GRID_SIZE) % GRID_SIZE;
  head.y = (head.y + GRID_SIZE) % GRID_SIZE;

  // Перевірка зіткнення з тілом (не враховуємо хвіст — він зрушиться)
  for (uint8_t i = 0; i < snakeLen - 1; i++) {
    if (snake[i].x == head.x && snake[i].y == head.y) {
      gameState = STATE_GAMEOVER;
      digitalWrite(LED_RED, HIGH);
      displayDash();
      return;
    }
  }

  // Чи з'їли яблуко?
  bool ate = (head.x == apple.x && head.y == apple.y);

  // Зрушуємо тіло
  if (ate) {
    if (snakeLen < MAX_SNAKE) snakeLen++;
    score++;
    displayScore();
  }
  for (uint8_t i = snakeLen - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  snake[0] = head;

  if (ate) spawnApple();
}

// ============================================================
//  ВІДМАЛЬОВУВАННЯ КАДРУ
// ============================================================

void buildFrame() {
  clearFrame();

  // Яблуко (мигає)
  if (appleVisible) setPixel(apple.x, apple.y, true);

  // Змійка
  for (uint8_t i = 0; i < snakeLen; i++) {
    setPixel(snake[i].x, snake[i].y, true);
  }
}

// ============================================================
//  ДЖОЙСТИК
// ============================================================

void readJoystick() {
  int vx = analogRead(JOY_X);
  int vy = analogRead(JOY_Y);

  // VRX: ліво/право
  if (vx < (2048 - JOY_THRESHOLD))      nextDir = DIR_LEFT;
  else if (vx > (2048 + JOY_THRESHOLD)) nextDir = DIR_RIGHT;
  // VRY: вверх/вниз
  else if (vy < (2048 - JOY_THRESHOLD)) nextDir = DIR_UP;
  else if (vy > (2048 + JOY_THRESHOLD)) nextDir = DIR_DOWN;
}

void readButton() {
  if (digitalRead(JOY_BTN) == LOW) {
    unsigned long now = millis();
    if (now - lastBtnPress < BTN_DEBOUNCE) return;
    lastBtnPress = now;

    if (gameState == STATE_GAMEOVER || gameState == STATE_WIN) {
      initGame(); // скидання
    } else if (gameState == STATE_PAUSE) {
      gameState = STATE_RUNNING;
    } else if (gameState == STATE_RUNNING) {
      gameState = STATE_PAUSE;
    }
  }
}

// ============================================================
//  SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(115200);

  // 7-seg
  pinMode(SSD_LATCH, OUTPUT);
  pinMode(SSD_DATA,  OUTPUT);
  pinMode(SSD_CLK,   OUTPUT);

  // Matrix
  pinMode(MTX_LATCH, OUTPUT);
  pinMode(MTX_DATA,  OUTPUT);
  pinMode(MTX_CLK,   OUTPUT);

  // Joystick
  pinMode(JOY_BTN, INPUT_PULLUP);
  analogReadResolution(12); // ESP32: 12-біт ADC (0–4095)

  // Red LED
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);

  randomSeed(analogRead(34)); // плаваючий пін для seed

  initGame();
}

void loop() {
  unsigned long now = millis();

  readButton();

  if (gameState == STATE_RUNNING) {
    readJoystick();

    // Крок гри
    if (now - lastStep >= GAME_SPEED_MS) {
      lastStep = now;
      gameStep();
    }

    // Мигання яблука
    if (now - lastBlink >= APPLE_BLINK_MS) {
      lastBlink = now;
      appleVisible = !appleVisible;
    }
  }

  // Будуємо і відображаємо кадр
  if (gameState != STATE_GAMEOVER) {
    buildFrame();
  } else {
    // Game over: вся матриця мигає
    if ((now / 300) % 2 == 0) {
      memset(frameBuffer, 0xFF, sizeof(frameBuffer));
    } else {
      clearFrame();
    }
  }

  if (gameState == STATE_WIN) {
    // Перемога: змійка заповнила поле — повільно мигаємо всю матрицю
    if ((now / 500) % 2 == 0) {
      memset(frameBuffer, 0xFF, sizeof(frameBuffer));
    } else {
      clearFrame();
    }
  }

  refreshMatrix();
}
