/**
 * =====================================================================
 *  Snake Game — Freenove ESP32 WROVER + FreeRTOS
 * =====================================================================
 *
 *  Hardware:
 *   • 8×8 LED Matrix    via 2× daisy-chained 74HC595  (U1 = Rows, U2 = Cols)
 *   • 1-digit 7-Segment 2x via 2× daisy-chained 74HC595  (U1 = Tens, U2 = Units)
 *   • Analog Joystick   (VRX, VRY, SW push-button — INPUT_PULLUP)
 *   • Red  LED (active HIGH) ─ Game-Over indicator
 *   • Blue LED (active HIGH) ─ Paused / Win indicator
 *
 *  FreeRTOS Architecture (dual-core):
 *  ┌────────────┬────────────────────────────────────────────────────────┐
 *  │  Core 1    │  taskDisplay  (configMAX_PRIORITIES-1)                 │
 *  │            │   Multiplexes 8×8 matrix at ≈83 Hz (zero flicker).     │
 *  │            │   Refreshes 7-seg once per full scan.                  │
 *  │            │   Reads a mutex-protected snapshot of matrixBuffer[].  │
 *  ├────────────┼────────────────────────────────────────────────────────┤
 *  │  Core 0    │  taskGameLogic  (priority 2)                           │
 *  │            │   Polls joystick; moves snake at SNAKE_SPEED_MS.       │
 *  │            │   Handles collisions, scoring, state transitions.      │
 *  │            │   Writes matrixBuffer[] under matrixMutex.             │
 *  └────────────┴────────────────────────────────────────────────────────┘
 *
 *  HOW TO USE:
 *   1. Fill in every '#define 0' with your actual ESP32 GPIO pin numbers.
 *   2. Set isCommonAnode7Seg  to match your 7-segment display type.
 *   3. Set isCommonAnodeMatrix to match your LED matrix type.
 *   4. Flash to ESP32 and play!
 *
 *  GAME CONTROLS:
 *   Joystick stick  — steer the snake (wrap-around map, no walls)
 *   Button press    — PAUSED → start / PLAYING → pause / GAMEOVER|WIN → restart
 *
 *  WIN  : snake fills all 64 cells → Blue LED blinks
 *  LOSE : snake bites itself       → Red LED lights solid
 * =====================================================================
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>   // memcpy

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 1 — PIN DEFINITIONS
//  Replace every 0 with your actual GPIO pin numbers before flashing!
// ─────────────────────────────────────────────────────────────────────────────

/* Snake game */

// ----- 74HC595 (seve g-ment display) -----
#define SSD_HC_DATA   13
#define SSD_HC_LATCH  18
#define SSD_HC_CLOCK  19

// ----- 74HC595 (led matrix 8x8) -----
#define LM_HC_DATA    25
#define LM_HC_LATCH   26
#define LM_HC_CLOCK   27

// ----- LEDs (active HIGH) -----
#define LED_R  21    // Red  LED → Game Over
#define LED_B  22    // Blue LED → Paused | Win

// ----- Joystick -----
#define VRX  34      // Analog X axis  (ESP32 12-bit ADC → 0–4095)
#define VRY  35      // Analog Y axis
#define SW   32      // Push-button   (INPUT_PULLUP; pressed = LOW)

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 2 — CONFIGURABLE HARDWARE LOGIC FLAGS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * true  = 7-segment displays are COMMON ANODE  (segment pin LOW  → segment ON)
 * false = 7-segment displays are COMMON CATHODE (segment pin HIGH → segment ON)
 * The lookup table is defined for common anode; bits are inverted automatically
 * when this flag is false.
 */
bool isCommonAnode7Seg = true;

/**
 * true  = LED matrix is COMMON ANODE  (row pin = anode,   column pin = cathode)
 * false = LED matrix is COMMON CATHODE (row pin = cathode, column pin = anode)
 * All row/column drive logic is derived from this flag at runtime.
 */
bool isCommonAnodeMatrix = true;

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 3 — GAME & TIMING CONSTANTS
// ─────────────────────────────────────────────────────────────────────────────

static const uint8_t  INITIAL_SNAKE_LENGTH = 3;     ///< Starting snake length (cells)
static const uint8_t  NUM_APPLES           = 1;     ///< Simultaneous apples on the board
static const uint32_t APPLE_BLINK_MS       = 400;   ///< Apple blink toggle period (ms)
static const uint32_t SNAKE_SPEED_MS       = 250;   ///< Milliseconds between snake steps
static const uint32_t DEBOUNCE_MS          = 50;    ///< Button debounce window (ms)
static const uint32_t WIN_BLINK_MS         = 300;   ///< Blue LED blink rate on WIN (ms)
/**
 * How long each row is lit during one multiplexing pass (microseconds).
 * Full scan: 8 rows × 1500 µs = 12 ms → ≈ 83 Hz refresh (flicker-free).
 */
static const uint32_t MX_ROW_HOLD_US = 1500;

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 4 — 7-SEGMENT LOOKUP TABLE  (Common Anode, active-LOW segments)
//
//  Bit layout sent MSB-first to the 74HC595:
//    Bit 7   6   5   4   3   2   1   0
//     dp   g   f   e   d   c   b   a
//
//        ─ a ─
//       |     |
//       f     b
//       |     |
//        ─ g ─
//       |     |
//       e     c
//       |     |
//        ─ d ─  • dp
// ─────────────────────────────────────────────────────────────────────────────

static const uint8_t SEG_DIGITS_CA[10] = {
    0b11000000,  // 0  segments: a b c d e f
    0b11111001,  // 1  segments: b c
    0b10100100,  // 2  segments: a b d e g
    0b10110000,  // 3  segments: a b c d g
    0b10011001,  // 4  segments: b c f g
    0b10010010,  // 5  segments: a c d f g
    0b10000010,  // 6  segments: a c d e f g
    0b11111000,  // 7  segments: a b c
    0b10000000,  // 8  segments: a b c d e f g
    0b10010000   // 9  segments: a b c d f g
};

/// All segments OFF for a common-anode display (all pins HIGH = no current path).
static const uint8_t SEG_BLANK_CA = 0xFF;

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 5 — TYPE DEFINITIONS
// ─────────────────────────────────────────────────────────────────────────────

/// Overall game-state machine.
enum GameState : uint8_t {
    STATE_PAUSED,    ///< Idle; Blue LED solid ON.
    STATE_PLAYING,   ///< Snake actively moving; both LEDs OFF.
    STATE_GAMEOVER,  ///< Self-bite detected; Red LED solid ON.
    STATE_WIN        ///< Board completely filled; Blue LED blinking.
};

/// Snake movement direction.
enum Direction : uint8_t {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
};

/// A cell coordinate on the 8×8 grid.
struct Point {
    int8_t x;   ///< Column  0–7  (left → right)
    int8_t y;   ///< Row     0–7  (top  → bottom)
};

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 6 — SHARED STATE  (both tasks; protected by matrixMutex where noted)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Display buffer for the 8×8 matrix.
 * matrixBuffer[row] is a column bitmask — bit N = 1 means column-N LED should
 * be ON in that row.
 * Written by taskGameLogic (under mutex), read by taskDisplay (under mutex).
 */
volatile uint8_t matrixBuffer[8] = {0};

/**
 * Score value driven to the 7-segment display.
 * uint8_t reads/writes are atomic on 32-bit ARM → no mutex required.
 */
volatile uint8_t displayScore = 0;

/// Mutex protecting matrixBuffer[].
SemaphoreHandle_t matrixMutex = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 7 — GAME STATE  (accessed only from taskGameLogic)
// ─────────────────────────────────────────────────────────────────────────────

static GameState gameState = STATE_PAUSED;

/**
 * Snake body stored as a circular (ring) buffer.
 *
 *   snakeBody[snakeTail] = oldest segment (tail tip).
 *   snakeBody[snakeHead] = newest segment (head).
 *
 * Valid body cells occupy indices:
 *   (snakeTail + 0) % 64  …  (snakeTail + snakeLen - 1) % 64  == snakeHead
 *
 * Moving WITHOUT eating:
 *   snakeHead = (snakeHead + 1) % 64  — write new head cell
 *   snakeTail = (snakeTail + 1) % 64  — discard old tail cell
 *   snakeLen unchanged.
 *
 * Moving WITH eating:
 *   snakeHead = (snakeHead + 1) % 64  — write new head cell
 *   snakeTail unchanged                — tail stays (snake grows)
 *   snakeLen++
 */
static Point    snakeBody[64];
static uint8_t  snakeHead = 0;           ///< Index of the head in snakeBody[]
static uint8_t  snakeTail = 0;           ///< Index of the tail in snakeBody[]
static uint8_t  snakeLen  = 0;           ///< Current snake length (cells)
static uint8_t  score     = 0;           ///< Apples eaten (raw counter)

static Direction snakeDir = DIR_RIGHT;   ///< Direction committed on the last step
static Direction nextDir  = DIR_RIGHT;   ///< Direction queued by the joystick

static Point applePos[NUM_APPLES];       ///< Grid coordinates of each apple
static bool  appleVisible = true;        ///< Current blink state of the apples

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 8 — FORWARD DECLARATIONS
// ─────────────────────────────────────────────────────────────────────────────

void    taskDisplay  (void* pvParam);
void    taskGameLogic(void* pvParam);

void    resetGame();
void    placeApple(uint8_t idx);
bool    isOnSnake(int8_t x, int8_t y);
void    rebuildMatrixBuffer();
uint8_t getSegByte(uint8_t digit);
void    writeSSD(uint8_t value);
void    writeMatrixRow(uint8_t row, uint8_t colData);
void    blankMatrix();

// ─────────────────────────────────────────────────────────────────────────────
//  SECTION 9 — SETUP
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000); // Дай ESP32 час "очухатись" після подачі живлення
    Serial.println("--- Booting Snake Game ---");

    // Ініціалізація пінів
    pinMode(SSD_HC_DATA,  OUTPUT);
    pinMode(SSD_HC_LATCH, OUTPUT);
    pinMode(SSD_HC_CLOCK, OUTPUT);
    pinMode(LM_HC_DATA,   OUTPUT);
    pinMode(LM_HC_LATCH,  OUTPUT);
    pinMode(LM_HC_CLOCK,  OUTPUT);
    pinMode(LED_R, OUTPUT); digitalWrite(LED_R, LOW);
    pinMode(LED_B, OUTPUT); digitalWrite(LED_B, HIGH);
    pinMode(SW, INPUT_PULLUP);
    analogReadResolution(12);

    // Створення м'ютекса
    matrixMutex = xSemaphoreCreateMutex();
    if (matrixMutex != nullptr) {
        Serial.println("1. Mutex OK");
    } else {
        Serial.println("!!! Mutex FAIL");
    }

    // Початковий стан гри
    resetGame();
    writeSSD(0);

    // Створюємо задачі ТІЛЬКИ ОДИН РАЗ
    // Обидві на Core 1 для чистоти тесту
    xTaskCreatePinnedToCore(taskDisplay, "Display", 2048, nullptr, configMAX_PRIORITIES - 1, nullptr, 1);
    Serial.println("2. Display Task started");

    xTaskCreatePinnedToCore(taskGameLogic, "GameLogic", 4096, nullptr, 2, nullptr, 1);
    Serial.println("3. GameLogic Task started");
    
    Serial.println("--- Setup Finished ---");
}

// loop() is intentionally empty — all work lives in FreeRTOS tasks.
void loop() {
    vTaskDelay(portMAX_DELAY);
}

// ─────────────────────────────────────────────────────────────────────────────
//  TASK 1 — DISPLAY MULTIPLEXING  (Core 1, max priority)
// ─────────────────────────────────────────────────────────────────────────────
/**
 * Cycles through all 8 matrix rows continuously, illuminating each row for
 * MX_ROW_HOLD_US microseconds before moving to the next.
 *
 * Full-cycle duration: 8 × 1500 µs = 12 ms → ≈ 83 Hz  (well above flicker
 * threshold; human eye perceives flicker below ~50 Hz).
 *
 * At the start of each scan cycle the task snapshots matrixBuffer[] under
 * the mutex.  If the mutex is momentarily busy the task reuses the previous
 * snapshot — at most one frame of visual lag, imperceptible to the player.
 *
 * The 7-segment display is refreshed once per full scan (every ~12 ms).
 * This far exceeds the ~30 Hz human-readable update rate required.
 */
void taskDisplay(void* pvParam) {
    (void)pvParam;
    static uint8_t localBuf[8] = {0};
    uint8_t row = 0;

    while (true) {
        // 1. Snapshot the buffer and update 7-seg once per full frame
        if (row == 0) {
            // Use 0 ticks to avoid blocking the display thread if mutex is busy
            if (xSemaphoreTake(matrixMutex, 0) == pdTRUE) {
                memcpy(localBuf, (const uint8_t*)matrixBuffer, 8);
                xSemaphoreGive(matrixMutex);
            }
            writeSSD(displayScore);
        }

        // 2. Blank matrix right BEFORE writing to prevent ghosting between rows
        blankMatrix();
        
        // 3. Light up the current row
        writeMatrixRow(row, localBuf[row]);

        // 4. Advance row counter
        row = (row + 1) % 8;

        // 5. CRITICAL FIX: Sleep for 1 tick (~1ms) to let GameLogic and IDLE tasks run
        // 8 rows * 1ms = 8ms per frame -> ~125 Hz refresh rate (flicker-free)
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  TASK 2 — GAME LOGIC  (Core 0, priority 2)
// ─────────────────────────────────────────────────────────────────────────────
/**
 * Polls inputs and advances game state at a 10 ms granularity.
 * The snake itself only steps every SNAKE_SPEED_MS ms (millis-delta timer).
 *
 * Responsibilities:
 *  ① Debounce joystick button → generate single-tick freshPress events.
 *  ② Map button press to state transitions (PAUSED ↔ PLAYING, restart).
 *  ③ Read joystick axes and queue a direction change for the next step.
 *  ④ At each SNAKE_SPEED_MS tick:
 *       a. Commit queued direction.
 *       b. Compute new head position (with toroidal wrap-around).
 *       c. Detect apple collection (grow snake, update score, place new apple).
 *       d. Detect self-collision → GAMEOVER.
 *       e. Detect win condition (snakeLen == 64) → WIN.
 *       f. Push updated snake + apple state to matrixBuffer[] via rebuildMatrixBuffer().
 *  ⑤ Toggle apple blink every APPLE_BLINK_MS.
 *  ⑥ Blink Blue LED in WIN state every WIN_BLINK_MS.
 */
void taskGameLogic(void* pvParam) {
    (void)pvParam;

    // ── Interval timers ───────────────────────────────────────────────────────
    uint32_t lastMoveTime   = millis();
    uint32_t lastAppleBlink = millis();
    uint32_t lastWinBlink   = millis();
    bool     winLedState    = false;

    // ── Button debounce state ─────────────────────────────────────────────────
    // SW is INPUT_PULLUP → idle = HIGH, pressed = LOW.
    bool     lastRawBtn    = true;   // Most-recent raw GPIO reading
    bool     stableBtn     = true;   // Debounced stable reading
    uint32_t lastBounceMs  = 0;      // Timestamp of last raw-signal change
    bool     freshPress    = false;  // Set true for exactly one 10 ms tick on press

    while (true) {
        Serial.println("Logic Heartbeat");
        uint32_t now = millis();

        // ====================================================================
        //  ① BUTTON DEBOUNCE
        //  Classic "time-since-last-change" algorithm.
        //  freshPress is true only on the tick where a stable LOW is detected.
        // ====================================================================
        bool rawBtn = (bool)digitalRead(SW);
        freshPress  = false;

        if (rawBtn != lastRawBtn) {         // Any edge resets the debounce timer
            lastBounceMs = now;
            lastRawBtn   = rawBtn;
        }

        if ((now - lastBounceMs) >= DEBOUNCE_MS) {
            if (stableBtn != rawBtn) {      // Signal has been stable long enough
                stableBtn = rawBtn;
                if (stableBtn == LOW) {     // LOW = pressed (active-low pull-up)
                    freshPress = true;
                    Serial.println("BUTTON PRESSED!"); // ДОДАЙ ЦЕЙ РЯДОК
                }
            }
        }

        // ====================================================================
        //  ② STATE TRANSITIONS ON BUTTON PRESS
        // ====================================================================
        if (freshPress) {
            switch (gameState) {

                case STATE_PAUSED:
                    // Start or resume the game
                    gameState    = STATE_PLAYING;
                    digitalWrite(LED_B, LOW);
                    lastMoveTime = now;  // Reset move timer → no instant step on resume
                    break;

                case STATE_PLAYING:
                    // Pause the game
                    gameState = STATE_PAUSED;
                    digitalWrite(LED_B, HIGH);
                    break;

                case STATE_GAMEOVER:  // Both GAMEOVER and WIN restart on button press
                case STATE_WIN:
                    resetGame();
                    gameState   = STATE_PAUSED;
                    winLedState = false;
                    digitalWrite(LED_R, LOW);
                    digitalWrite(LED_B, HIGH);  // Return to paused indicator
                    break;
            }
        }

        // ====================================================================
        //  ③④⑤  PLAYING STATE — joystick, apple blink, snake movement
        // ====================================================================
        if (gameState == STATE_PLAYING) {

            // ── ③ Joystick direction input ──────────────────────────────────
            // ESP32 12-bit ADC.  Joystick centre ≈ 2048.
            // Dead-zone: ±600 LSB (~30 % of half-range) prevents drift noise.
            //
            // Polarity note:
            //   Pushing the stick UP usually drives VRY toward 0 (negative delta).
            //   If your stick feels inverted, negate vx or vy to match hardware.
            //
            int32_t vx = (int32_t)analogRead(VRX) - 2048;  // –2048…+2047
            int32_t vy = (int32_t)analogRead(VRY) - 2048;  // negative = toward row 0 (UP)
            const int32_t DZ = 600;

            if (abs(vx) >= abs(vy)) {
                // Horizontal axis dominant
                if (vx >  DZ && snakeDir != DIR_LEFT)  nextDir = DIR_RIGHT;
                if (vx < -DZ && snakeDir != DIR_RIGHT) nextDir = DIR_LEFT;
            } else {
                // Vertical axis dominant
                if (vy < -DZ && snakeDir != DIR_DOWN)  nextDir = DIR_UP;
                if (vy >  DZ && snakeDir != DIR_UP)    nextDir = DIR_DOWN;
            }

            // ── ⑤ Apple blink ────────────────────────────────────────────────
            if ((now - lastAppleBlink) >= APPLE_BLINK_MS) {
                lastAppleBlink = now;
                appleVisible   = !appleVisible;  // Toggle visibility
                rebuildMatrixBuffer();            // Push blink state to display buffer
            }

            // ── ④ Snake movement tick ─────────────────────────────────────────
            if ((now - lastMoveTime) >= SNAKE_SPEED_MS) {
                lastMoveTime = now;
                snakeDir     = nextDir;  // Commit the joystick-queued direction

                // -- Compute new head position (toroidal / wrap-around map) ---
                // The map has no walls; exiting one edge teleports to the other.
                Point newHead = snakeBody[snakeHead];
                switch (snakeDir) {
                    case DIR_UP:    newHead.y = (newHead.y - 1 + 8) & 7; break;
                    case DIR_DOWN:  newHead.y = (newHead.y + 1    ) & 7; break;
                    case DIR_LEFT:  newHead.x = (newHead.x - 1 + 8) & 7; break;
                    case DIR_RIGHT: newHead.x = (newHead.x + 1    ) & 7; break;
                }

                // -- Apple collection check -----------------------------------
                bool    ateApple    = false;
                uint8_t ateAppleIdx = 0;
                for (uint8_t i = 0; i < NUM_APPLES; i++) {
                    if (newHead.x == applePos[i].x && newHead.y == applePos[i].y) {
                        ateApple    = true;
                        ateAppleIdx = i;
                        break;
                    }
                }

                // -- Self-collision check -------------------------------------
                //
                // If NOT eating: the tail cell vacates this step, so we must
                //   SKIP it in the collision check (snake can legally "chase"
                //   its own tail).  Start loop at body index 1 (first segment
                //   after the tail).
                //
                // If EATING: the tail stays (snake grows), so we must CHECK it
                //   too.  Start loop at body index 0 (the tail itself).
                //
                bool    selfCollide = false;
                uint8_t startI      = ateApple ? 0u : 1u;
                for (uint8_t i = startI; i < snakeLen; i++) {
                    const Point& seg = snakeBody[(snakeTail + i) % 64];
                    if (seg.x == newHead.x && seg.y == newHead.y) {
                        selfCollide = true;
                        break;
                    }
                }

                // -- Resolve movement outcome ---------------------------------
                if (selfCollide) {
                    // ── GAME OVER ──────────────────────────────────────────
                    gameState = STATE_GAMEOVER;
                    digitalWrite(LED_R, HIGH);
                    // matrixBuffer unchanged → shows the death-state board.

                } else {
                    // ── VALID MOVE ─────────────────────────────────────────
                    // Place the new head into the ring buffer.
                    snakeHead            = (snakeHead + 1) % 64;
                    snakeBody[snakeHead] = newHead;

                    if (ateApple) {
                        // Snake grows by 1; tail stays.
                        snakeLen++;
                        score++;
                        displayScore = score;   // uint8_t write is atomic on ARM

                        if (snakeLen >= 64) {
                            // ── WIN ────────────────────────────────────────
                            // Board completely filled — snake length 64.
                            gameState    = STATE_WIN;
                            winLedState  = false;
                            lastWinBlink = now;
                            // Board is full; no new apple needed.
                        } else {
                            appleVisible = true;          // New apple starts visible
                            placeApple(ateAppleIdx);      // Random empty cell
                        }
                    } else {
                        // No growth: advance tail (snake slides forward).
                        snakeTail = (snakeTail + 1) % 64;
                    }

                    rebuildMatrixBuffer();
                }
            }

        // ====================================================================
        //  ⑥  WIN STATE — blink Blue LED
        // ====================================================================
        } else if (gameState == STATE_WIN) {
            if ((now - lastWinBlink) >= WIN_BLINK_MS) {
                lastWinBlink = now;
                winLedState  = !winLedState;
                digitalWrite(LED_B, winLedState ? HIGH : LOW);
            }
        }

        // 10 ms polling granularity — fine-grained enough for responsive input,
        // light enough to leave plenty of headroom on Core 0.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  GAME HELPER FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Initialise (or re-initialise) all game state to the starting configuration.
 *
 * Initial layout:
 *   Snake: horizontal, row 3, tail at (0,3), head at (INITIAL_SNAKE_LENGTH-1, 3).
 *   Direction: RIGHT.
 *   Score: 0.
 *   Apples: randomly placed on empty cells.
 *
 * Called once from setup() and again whenever the player restarts.
 */
void resetGame() {
    snakeLen  = INITIAL_SNAKE_LENGTH;
    snakeTail = 0;
    snakeHead = INITIAL_SNAKE_LENGTH - 1;
    snakeDir  = DIR_RIGHT;
    nextDir   = DIR_RIGHT;
    score     = 0;
    displayScore = 0;
    appleVisible = true;

    // Lay the snake horizontally across the top-left area of row 3.
    // With INITIAL_SNAKE_LENGTH = 3: cells (0,3), (1,3), (2,3) — head at (2,3).
    for (uint8_t i = 0; i < INITIAL_SNAKE_LENGTH; i++) {
        snakeBody[i].x = (int8_t)i;
        snakeBody[i].y = 3;
    }

    for (uint8_t i = 0; i < NUM_APPLES; i++) placeApple(i);
    rebuildMatrixBuffer();
}

/**
 * Returns true if the cell (x, y) is currently occupied by any snake segment.
 * Used by placeApple() to find empty cells.
 */
bool isOnSnake(int8_t x, int8_t y) {
    for (uint8_t i = 0; i < snakeLen; i++) {
        const Point& p = snakeBody[(snakeTail + i) % 64];
        if (p.x == x && p.y == y) return true;
    }
    return false;
}

/**
 * Place apple[idx] at a random cell that is not occupied by the snake or any
 * other apple.
 *
 * On the ESP32, Arduino's random() uses the hardware True Random Number
 * Generator (TRNG) — no manual seeding is required.
 *
 * @param idx  Index into applePos[] to update (0 … NUM_APPLES-1).
 */
void placeApple(uint8_t idx) {
    if (snakeLen >= 64) return;  // Board completely full

    int8_t ax, ay;
    bool busy;
    uint8_t attempts = 0;

    do {
        ax = (int8_t)random(8);
        ay = (int8_t)random(8);
        busy = isOnSnake(ax, ay);

        // Check if landing on another apple
        for (uint8_t j = 0; j < NUM_APPLES && !busy; j++) {
            if (j != idx && applePos[j].x == ax && applePos[j].y == ay) {
                busy = true;
            }
        }

        attempts++;
        // FAIL-SAFE: If RNG is stuck during boot, force a linear scan
        if (attempts > 50) {
            for(int8_t y = 0; y < 8 && busy; y++) {
                for(int8_t x = 0; x < 8 && busy; x++) {
                    if(!isOnSnake(x, y)) {
                        ax = x; 
                        ay = y;
                        busy = false; // Force exit
                    }
                }
            }
        }
    } while (busy);

    applePos[idx].x = ax;
    applePos[idx].y = ay;
}

/**
 * Rebuild matrixBuffer[] to reflect the current snake and apple positions,
 * then atomically copy the result to the shared display buffer under matrixMutex.
 *
 * Must be called whenever the visual state changes:
 *   • After every snake step (move or grow).
 *   • After placing a new apple.
 *   • After toggling appleVisible (blink tick).
 */
void rebuildMatrixBuffer() {
    uint8_t buf[8] = {0};

    // Draw every snake segment.
    for (uint8_t i = 0; i < snakeLen; i++) {
        const Point& p = snakeBody[(snakeTail + i) % 64];
        // Bounds guard — coordinates should always be 0-7, but be defensive.
        if ((uint8_t)p.x < 8 && (uint8_t)p.y < 8) {
            buf[p.y] |= (uint8_t)(1u << p.x);
        }
    }

    // Draw apples (only when visible — they blink to stand out from the snake).
    if (appleVisible) {
        for (uint8_t i = 0; i < NUM_APPLES; i++) {
            if ((uint8_t)applePos[i].x < 8 && (uint8_t)applePos[i].y < 8) {
                buf[applePos[i].y] |= (uint8_t)(1u << applePos[i].x);
            }
        }
    }

    // Write to shared display buffer under mutex.
    if (xSemaphoreTake(matrixMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy((uint8_t*)matrixBuffer, buf, 8);
        xSemaphoreGive(matrixMutex);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  DISPLAY DRIVER FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Return the 8-bit value to load into a 74HC595 for a single digit.
 *
 * @param digit  0–9 for a numeral; any other value produces a blank display.
 * @return Byte to shift out MSB-first; accounts for isCommonAnode7Seg.
 */
uint8_t getSegByte(uint8_t digit) {
    uint8_t caVal = (digit <= 9) ? SEG_DIGITS_CA[digit] : SEG_BLANK_CA;
    // Common cathode: invert all bits (segments now active HIGH).
    return isCommonAnode7Seg ? caVal : (uint8_t)~caVal;
}

/**
 * Drive the two 7-segment displays via the SSD daisy-chained 74HC595 pair.
 *
 * Shift order (per specification):
 *   Step 1 — Shift UNITS byte  →  travels through U1, lands in U2.
 *   Step 2 — Shift TENS  byte  →  stays in U1.
 *   Step 3 — Rising edge on LATCH latches both registers simultaneously.
 *
 * Physical wiring implied by this order:
 *   U1 output (Q0-Q7) drives the TENS  7-segment display.
 *   U2 output (Q0-Q7) drives the UNITS 7-segment display.
 *
 * Leading-zero suppression: scores 0–9 show " X" (tens digit blanked).
 *
 * @param value  Score to display, clamped to 0–99.
 */
void writeSSD(uint8_t value) {
    if (value > 99) value = 99;

    uint8_t lowByte  = getSegByte(value % 10);         // Units digit
    uint8_t highByte = (value < 10)
                       ? getSegByte(255)               // Blank (suppress leading zero)
                       : getSegByte(value / 10);       // Tens digit

    digitalWrite(SSD_HC_LATCH, LOW);
    shiftOut(SSD_HC_DATA, SSD_HC_CLOCK, MSBFIRST, lowByte);   // → U2 (units display)
    shiftOut(SSD_HC_DATA, SSD_HC_CLOCK, MSBFIRST, highByte);  // → U1 (tens  display)
    digitalWrite(SSD_HC_LATCH, HIGH);  // Latch both registers simultaneously
}

/**
 * Drive a single row of the 8×8 LED matrix via the LM daisy-chained pair.
 *
 * Shift order:
 *   Step 1 — Shift COLUMNS byte  →  travels through U1, lands in U2.
 *   Step 2 — Shift ROWS    byte  →  stays in U1.
 *   Step 3 — Rising edge on LATCH.
 *
 * Physical wiring implied:
 *   U1 output (Q0-Q7) → Row    pins (Q0=Row0 … Q7=Row7)
 *   U2 output (Q0-Q7) → Column pins (Q0=Col0 … Q7=Col7)
 *
 * Common-Anode matrix (isCommonAnodeMatrix = true):
 *   Row   active = pin driven HIGH  → rowByte = (1 << row)
 *   LED   on     = column cathode LOW → colByte = ~colData
 *
 * Common-Cathode matrix (isCommonAnodeMatrix = false):
 *   Row   active = pin driven LOW   → rowByte = ~(1 << row)
 *   LED   on     = column anode HIGH → colByte = colData
 *
 * @param row      Row index 0–7 to illuminate.
 * @param colData  Bitmask of columns to light in this row (bit N = 1 → ON).
 */
void writeMatrixRow(uint8_t row, uint8_t colData) {
    uint8_t rowByte, colByte;

    if (isCommonAnodeMatrix) {
        rowByte = (uint8_t)(1u << row);   // One anode HIGH = row selected
        colByte = (uint8_t)~colData;      // Cathode LOW = LED on (inverted)
    } else {
        rowByte = (uint8_t)~(1u << row);  // One cathode LOW = row selected
        colByte = colData;                 // Anode HIGH = LED on (non-inverted)
    }

    digitalWrite(LM_HC_LATCH, LOW);
    shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, colByte);  // Columns → U2
    shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, rowByte);  // Rows    → U1
    digitalWrite(LM_HC_LATCH, HIGH);
}

/**
 * Set all matrix row and column outputs to their inactive (LEDs-off) levels.
 *
 * Called after each full 8-row scan to prevent the last-activated row from
 * ghosting into the idle gap before the next scan cycle begins.
 *
 * Inactive levels:
 *   Common anode:   rows = 0x00 (no anode HIGH),   cols = 0xFF (no cathode LOW)
 *   Common cathode: rows = 0xFF (no cathode LOW),  cols = 0x00 (no anode HIGH)
 */
void blankMatrix() {
    uint8_t rowByte = isCommonAnodeMatrix ? 0x00u : 0xFFu;
    uint8_t colByte = isCommonAnodeMatrix ? 0xFFu : 0x00u;

    digitalWrite(LM_HC_LATCH, LOW);
    shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, colByte);
    shiftOut(LM_HC_DATA, LM_HC_CLOCK, MSBFIRST, rowByte);
    digitalWrite(LM_HC_LATCH, HIGH);
}
