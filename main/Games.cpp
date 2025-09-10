#include "RotaryEncoder.h"
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "img.h"
#include "LED.h"
#include "Buzzer.h"
#include "Alarm.h"
#include "weather.h"
#include "performance.h"
#include "DS18B20.h"
#include "animation.h"
#include "Games.h"
#include "Alarm.h"
#include "MQTT.h"
#include <vector> // For std::vector


// Snake Game Constants
#define SNAKE_GRID_WIDTH  20
#define SNAKE_GRID_HEIGHT 20
#define SNAKE_CELL_SIZE   10 // Pixels per cell
#define SNAKE_START_X     (SCREEN_WIDTH / 2 - (SNAKE_GRID_WIDTH * SNAKE_CELL_SIZE) / 2)
#define SNAKE_START_Y     (SCREEN_HEIGHT / 2 - (SNAKE_GRID_HEIGHT * SNAKE_CELL_SIZE) / 2)
#define SNAKE_INITIAL_LENGTH 3
#define SNAKE_GAME_SPEED_MS 200 // Milliseconds per frame

enum SnakeDirection {
  SNAKE_UP,
  SNAKE_DOWN,
  SNAKE_LEFT,
  SNAKE_RIGHT
};

struct Point {
  int x;
  int y;
};

std::vector<Point> snakeBody;
Point food;
SnakeDirection currentDirection;
bool gameOver;
int snakeScore;

// --- Layout Configuration ---
static const int ICON_SIZE = 200;
static const int ICON_SPACING = 220;
static const int SCREEN_WIDTH = 240;
static const int SCREEN_HEIGHT = 240;

// Calculated layout values
static const int ICON_Y_POS = (SCREEN_HEIGHT / 2) - (ICON_SIZE / 2);
static const int TRIANGLE_BASE_Y = ICON_Y_POS - 5;
static const int TRIANGLE_PEAK_Y = TRIANGLE_BASE_Y - 20;
static const int INITIAL_X_OFFSET = (SCREEN_WIDTH / 2) - (ICON_SIZE / 2);
static const uint8_t ANIMATION_STEPS = 12;

// --- State Variables ---
int16_t game_display = INITIAL_X_OFFSET;
int16_t game_picture_flag = 0;

// --- Function Prototypes ---
void ConwayGame();
void BuzzerTapGame();
void TimeChallengeGame();
void drawGameIcons(int16_t offset);

// --- Button Detection (Strictly copied from Buzzer.cpp) ---
bool gamesDetectDoubleClick(bool reset = false) {
  static unsigned long lastClickTime = 0;
  if (reset) {
      lastClickTime = 0;
      return false; // Not a double click, just reset
  }
  unsigned long currentTime = millis();
  if (currentTime - lastClickTime < 500) { // 500ms 内两次点击
    lastClickTime = 0;
    return true;
  }
  lastClickTime = currentTime;
  return false;
}

// --- Button Debouncing Variables ---
static unsigned long lastButtonDebounceTime = 0;
static int lastDebouncedButtonState = HIGH; // HIGH = not pressed, LOW = pressed
const int BUTTON_DEBOUNCE_DELAY = 50; // ms

#define ENCODER_SW 25 // Button pin, copied from RotaryEncoder.cpp

// Function to get debounced button state (true if button is currently pressed and debounced)
bool getDebouncedButtonState() {
    int reading = digitalRead(ENCODER_SW); // Raw button read

    if (reading != lastDebouncedButtonState) {
        lastButtonDebounceTime = millis();
    }

    if ((millis() - lastButtonDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
        if (reading != lastDebouncedButtonState) { // State has settled
            lastDebouncedButtonState = reading;
        }
    }
    return (lastDebouncedButtonState == LOW); // Return true if button is pressed
}

// --- Menu Definition ---
struct GameItem {
    const char *name;
    const uint16_t *image;
    void (*function)(); // Function pointer to the game
};

// Game list with placeholder icons and function pointers
const GameItem gameItems[] = {
    {"Conway's Game", Conway, ConwayGame},
    // {"Breakout", Conway, breakoutGame},
    // {"Car Dodger", Conway, carDodgerGame}, // Using car icon as placeholder for now
 // Using Conway icon as placeholder for now

    // {"Snake Game", snake, tanchisheGame},

    {"Flappy Bird", bird_big, flappy_bird_game}, // Using Dinasor icon as placeholder
    {"Buzzer Tap", Sound, BuzzerTapGame},
    {"Time Challenge", Timer, TimeChallengeGame},
    // {"Dino Game", Dinasor, dinoGame}, // Placeholder icon
};
const uint8_t GAME_ITEM_COUNT = sizeof(gameItems) / sizeof(gameItems[0]);

// --- Drawing Functions ---
void drawGameIcons(int16_t offset) {
    menuSprite.fillSprite(TFT_BLACK);

    // Selection triangle indicator (moved to bottom)
    int16_t triangle_x = offset + (game_picture_flag * ICON_SPACING) + (ICON_SIZE / 2);
    // New Y-coordinates for triangle at the bottom, pointing downwards
    // Peak at SCREEN_HEIGHT - 5, Base at SCREEN_HEIGHT - 25
    menuSprite.fillTriangle(triangle_x, SCREEN_HEIGHT - 25, triangle_x - 12, SCREEN_HEIGHT - 5, triangle_x + 12, SCREEN_HEIGHT - 5, TFT_WHITE);

    // Icons
    for (int i = 0; i < GAME_ITEM_COUNT; i++) {
        int16_t x = offset + (i * ICON_SPACING);
        if (x >= -ICON_SIZE && x < SCREEN_WIDTH) {
            menuSprite.pushImage(x, ICON_Y_POS, ICON_SIZE, ICON_SIZE, gameItems[i].image);
        }
    }

    // Text
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(1);
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.drawString("GAMES:", 10, 10);
    menuSprite.drawString(gameItems[game_picture_flag].name, 60, 10); // Corrected variable
    // Removed: menuSprite.drawString("Select", 10, 220);

    menuSprite.pushSprite(0, 0);
}

// --- Main Game Menu Function ---
void GamesMenu() {
    tft.fillScreen(TFT_BLACK); // Clear screen directly

    // Reset state for the game menu
    game_picture_flag = 0;
    game_display = INITIAL_X_OFFSET;
    drawGameIcons(game_display);

    gamesDetectDoubleClick(true); // Reset double click state on entry

    // State for delayed single-click in GamesMenu
    static unsigned long gamesMenuLastClickTime = 0;
    static bool gamesMenuSingleClickPending = false;

    while (true) {
        if (exitSubMenu || g_alarm_is_ringing) { return; } // ADDED LINE
        if (readButtonLongPress()) { return; }
        int direction = readEncoder();
        if (direction != 0) {
            if (direction == 1) {
                game_picture_flag = (game_picture_flag + 1) % GAME_ITEM_COUNT;
            } else if (direction == -1) {
                game_picture_flag = (game_picture_flag == 0) ? GAME_ITEM_COUNT - 1 : game_picture_flag - 1;
            }
            tone(BUZZER_PIN, 1000 * (game_picture_flag + 1), 20);
            int16_t target_display = INITIAL_X_OFFSET - (game_picture_flag * ICON_SPACING);

            int16_t start_display = game_display; // Capture the starting position
            for (uint8_t i = 0; i <= ANIMATION_STEPS; i++) { // Loop from 0 to ANIMATION_STEPS
                float t = (float)i / ANIMATION_STEPS; // Progress from 0.0 to 1.0
                float eased_t = easeOutQuad(t); // Apply easing

                game_display = start_display + (target_display - start_display) * eased_t; // Calculate interpolated position

                drawGameIcons(game_display);
                vTaskDelay(pdMS_TO_TICKS(15)); // Keep delay for now
            }

            game_display = target_display;
            drawGameIcons(game_display);
        }

        if (readButton()) { // Use readButton() directly
            if (gamesDetectDoubleClick()) { // Double click detected
                gamesMenuSingleClickPending = false; // Cancel any pending single click
                return; // Exit GamesMenu
            } else {
                // This is a single click (or the first click of a potential double click)
                // Set flag and record time, then wait for DOUBLE_CLICK_WINDOW
                gamesMenuLastClickTime = millis();
                gamesMenuSingleClickPending = true;
            }
        }

        // Check if a pending single click should be executed
        if (gamesMenuSingleClickPending && (millis() - gamesMenuLastClickTime > 500)) { // DOUBLE_CLICK_WINDOW
            gamesMenuSingleClickPending = false; // Consume the pending single click
            
            tone(BUZZER_PIN, 2000, 50);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (gameItems[game_picture_flag].function) {
                gameItems[game_picture_flag].function();
            }
            tft.fillScreen(TFT_BLACK); // Clear screen directly
            drawGameIcons(game_display);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Breakout Game Implementation ---
#define PLATFORM_WIDTH  12
#define PLATFORM_HEIGHT 4
#define UPT_MOVE_NONE   0
#define UPT_MOVE_RIGHT  1
#define UPT_MOVE_LEFT   2
#define BLOCK_COLS      32
#define BLOCK_ROWS      5
#define BLOCK_COUNT     (BLOCK_COLS * BLOCK_ROWS)

typedef struct {
    float x;
    float y;
    float velX;
    float velY;
} s_ball;

// Placeholder bitmaps (replace with actual ones if available in img.h or define new ones)
// For simplicity, using simple 1-bit bitmaps as uint16_t arrays for pushImage
const uint16_t block_bitmap[] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // Example 8x8 white block
const uint16_t platform_bitmap[] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // Example 16x8 white platform
const uint16_t ball_bitmap[] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // Example 2x2 white ball

static uint8_t uptMove;
static s_ball ball;
static bool* blocks;
static uint8_t lives;
static uint32_t score;
static uint8_t platformX;

// Helper function to draw bitmaps (adapting from original draw_bitmap)
void draw_game_bitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h) {
    menuSprite.pushImage(x, y, w, h, bitmap);
}

// Helper function to draw strings (adapting from original draw_string)
void draw_game_string(const char *str, int16_t x, int16_t y) {
    menuSprite.drawString(str, x, y);
}

// Game draw function
void breakout_draw() {
    bool gameEnded = ((score >= BLOCK_COUNT) || (lives == 255));

    uint8_t platformXtmp = platformX;

    // Move platform
    if (uptMove == UPT_MOVE_RIGHT)
        platformXtmp += 3;
    else if (uptMove == UPT_MOVE_LEFT)
        platformXtmp -= 3;
    uptMove = UPT_MOVE_NONE;

    // Make sure platform stays on screen
    if (platformXtmp > 250) // Original code had > 250, which is likely an overflow check. Assuming 0-240 range.
        platformXtmp = 0;
    else if (platformXtmp > SCREEN_WIDTH - PLATFORM_WIDTH)
        platformXtmp = SCREEN_WIDTH - PLATFORM_WIDTH;

    // Draw platform
    draw_game_bitmap(platformXtmp, SCREEN_HEIGHT - 8, platform_bitmap, PLATFORM_WIDTH, 8);

    platformX = platformXtmp;

    // Move ball
    if (!gameEnded) {
        ball.x += ball.velX;
        ball.y += ball.velY;
    }

    bool blockCollide = false;
    const float ballX = ball.x;
    const uint8_t ballY = ball.y;

    // Block collision
    uint32_t idx = 0;
    for (uint8_t x = 0; x < BLOCK_COLS; x++) {
        for (uint8_t y = 0; y < BLOCK_ROWS; y++) {
            if (!blocks[idx] && ballX >= x * 4 && ballX < (x * 4) + 4 && ballY >= (y * 4) + 8 && ballY < (y * 4) + 8 + 4) {
                tone(BUZZER_PIN, 2000, 50); // Equivalent to buzzer_buzz(100, TONE_2KHZ, ...)
                // led_flash(LED_GREEN, 50, 255); // No direct equivalent, maybe toggle LED0
                blocks[idx] = true;
                blockCollide = true;
                score++;
            }
            idx++;
        }
    }

    // Side wall collision
    if (ballX > SCREEN_WIDTH - 2) {
        if (ballX > 240) // Original overflow check
            ball.x = 0;
        else
            ball.x = SCREEN_WIDTH - 2;
        ball.velX = -ball.velX;
    }
    if (ballX < 0) {
        ball.x = 0;
        ball.velX = -ball.velX;
    }

    // Platform collision
    bool platformCollision = false;
    if (!gameEnded && ballY >= SCREEN_HEIGHT - PLATFORM_HEIGHT && ballY < 240 && ballX >= platformX && ballX <= platformX + PLATFORM_WIDTH) {
        platformCollision = true;
        tone(BUZZER_PIN, 5000, 200); // Equivalent to buzzer_buzz(200, TONE_5KHZ, ...)
        ball.y = SCREEN_HEIGHT - PLATFORM_HEIGHT;
        if (ball.velY > 0)
            ball.velY = -ball.velY;
        ball.velX = ((float)random(0, RAND_MAX) / (RAND_MAX / 2.0)) - 1.0; // -1.0 to 1.0
    }

    // Top/bottom wall collision
    if (!gameEnded && !platformCollision && (ballY > SCREEN_HEIGHT - 2 || blockCollide)) {
        if (ballY > 240) { // Original overflow check
            tone(BUZZER_PIN, 2500, 200); // Equivalent to buzzer_buzz(200, TONE_2_5KHZ, ...)
            ball.y = 0;
        } else if (!blockCollide) {
            tone(BUZZER_PIN, 2000, 200); // Equivalent to buzzer_buzz(200, TONE_2KHZ, ...)
            ball.y = SCREEN_HEIGHT - 1;
            lives--;
        }
        ball.velY *= -1;
    }

    // Draw ball
    draw_game_bitmap(ball.x, ball.y, ball_bitmap, 2, 8); // Original was 2, 8. Assuming 2x8 pixels.

    // Draw blocks
    idx = 0;
    for (uint8_t x = 0; x < BLOCK_COLS; x++) {
        for (uint8_t y = 0; y < BLOCK_ROWS; y++) {
            if (!blocks[idx])
                draw_game_bitmap(x * 4, (y * 4) + 8, block_bitmap, 3, 8); // Original was 3, 8. Assuming 3x8 pixels.
            idx++;
        }
    }

    // Draw score
    char buff[10]; // Increased buffer size
    sprintf(buff, "%lu", score);
    draw_game_string(buff, 0, 0);

    // Draw lives
    if (lives != 255) {
        for (uint8_t i = 0; i < lives; i++)
            draw_game_bitmap((SCREEN_WIDTH - (3 * 8)) + (8 * i), 1, bird_big, 7, 8); // Using bird_big as placeholder for livesImg
    }

    // Game over / Win messages
    if (score >= BLOCK_COUNT)
        draw_game_string("WIN", 50, 32);
    if (lives == 255) // 255 because of overflow
        draw_game_string("GAME OVER", 34, 32);

    menuSprite.pushSprite(0, 0); // Push to screen
}

void breakoutGame() {
    tft.fillScreen(TFT_BLACK); // Clear screen

    // Initialize game variables
    ball.x = SCREEN_WIDTH / 2;
    ball.y = SCREEN_HEIGHT - 10;
    ball.velX = -0.5;
    ball.velY = -0.6;

    // Allocate memory for blocks
    blocks = (bool*)calloc(BLOCK_COUNT, sizeof(bool));
    if (blocks == NULL) {
        // Handle memory allocation error
        return;
    }

    lives = 3;
    score = 0;
    platformX = (SCREEN_WIDTH / 2) - (PLATFORM_WIDTH / 2);

    // Game loop
    while (true) {
        if (exitSubMenu || g_alarm_is_ringing) {
            free(blocks); // Free allocated memory
            return; // Exit game
        }

        // Button handling
        int direction = readEncoder();
        if (direction == 1) { // Right
            uptMove = UPT_MOVE_RIGHT;
        } else if (direction == -1) { // Left
            uptMove = UPT_MOVE_LEFT;
        }

        if (readButton()) {
            if (gamesDetectDoubleClick()) {
                free(blocks); // Free allocated memory
                return; // Exit game on double click
            }
        }

        breakout_draw(); // Draw current frame
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay for frame rate
    }
}

// --- Conway's Game of Life Implementation ---
#define GRID_SIZE 60
#define CELL_SIZE 4 // Pixels per cell
bool grid[GRID_SIZE][GRID_SIZE];
bool nextGrid[GRID_SIZE][GRID_SIZE];

// Function to initialize grid with random state
void initConwayGrid() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = (random(100) < 20); // 20% chance of being alive
        }
    }
}

// Function to draw the grid
void drawConwayGrid() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if (grid[i][j]) {
                tft.fillRect(j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE, TFT_GREEN);
            } else {
                tft.fillRect(j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE, TFT_BLACK);
            }
        }
    }
}

// Function to update the grid to the next generation
void updateConwayGrid() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            int liveNeighbors = 0;
            // Check 8 neighbors
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue; // Skip self
                    int ni = (i + x + GRID_SIZE) % GRID_SIZE; // Wrap around
                    int nj = (j + y + GRID_SIZE) % GRID_SIZE; // Wrap around
                    if (grid[ni][nj]) {
                        liveNeighbors++;
                    }
                }
            }

            if (grid[i][j]) { // Live cell
                if (liveNeighbors < 2 || liveNeighbors > 3) {
                    nextGrid[i][j] = false; // Dies
                } else {
                    nextGrid[i][j] = true; // Lives
                }
            } else { // Dead cell
                if (liveNeighbors == 3) {
                    nextGrid[i][j] = true; // Becomes alive
                } else {
                    nextGrid[i][j] = false; // Stays dead
                }
            }
        }
    }
    // Copy nextGrid to grid
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = nextGrid[i][j];
        }
    }
}

void ConwayGame() {
    tft.fillScreen(TFT_BLACK);
    initConwayGrid(); // Initialize with random state
    drawConwayGrid(); // Initial draw
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(35, 220);
    tft.print("Auto-running, Double-click to exit"); // Update instruction

    unsigned long lastUpdateTime = millis();
    const unsigned long UPDATE_INTERVAL = 200; // Update every 200ms

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            return; // Exit game
        }
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        unsigned long currentTime = millis();

        // Auto-run logic
        if (currentTime - lastUpdateTime > UPDATE_INTERVAL) {
            updateConwayGrid();
            drawConwayGrid();
            lastUpdateTime = currentTime;
        }

        // Button handling for exit
        if (readButtonLongPress()) { return; }
        // ADD THIS BLOCK
        if (readButton()) {
            if (gamesDetectDoubleClick()) {
                //exitSubMenu = true; // Signal to exit
                return; // Exit game
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay for responsiveness
    }
}



// --- Buzzer Tap Game Implementation ---
static int buzzerTapScore = 0;
const int TAP_WINDOW_MS = 300; // Time window to tap after tone starts
const int TONE_FREQ = 1000;
const int TONE_DURATION = 100; // ms

void BuzzerTapGame() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 50);
    tft.print("Score: ");
    tft.print(buzzerTapScore);
    tft.setTextSize(1);
    tft.setCursor(35, 140);
    tft.print("Tap after tone, Double-click to exit");

    buzzerTapScore = 0;

    unsigned long lastToneTime = 0;
    unsigned long nextToneInterval = random(1000, 3000); // Random interval between 1-3 seconds

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            noTone(BUZZER_PIN);
            return; // Exit game
        }
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        unsigned long currentTime = millis();

        // Play tone
        if (currentTime - lastToneTime > nextToneInterval) {
            tone(BUZZER_PIN, TONE_FREQ, TONE_DURATION);
            lastToneTime = currentTime;
            nextToneInterval = random(1000, 3000); // New random interval
        }

        if (readButtonLongPress()) {
            noTone(BUZZER_PIN); // Stop any ongoing tone
            return; // Exit
        } else if (readButton()) { // Handle short clicks for gameplay
            if (gamesDetectDoubleClick()) { // Double click detected
                //exitSubMenu = true; // Signal to exit
                noTone(BUZZER_PIN); // Stop any ongoing tone
                return; // Exit game
            }
            // Check if tap was within window
            if (currentTime - lastToneTime > 0 && currentTime - lastToneTime < TAP_WINDOW_MS) {
                buzzerTapScore++;
                tft.fillRect(100, 50, 100, 20, TFT_BLACK); // Clear old score
                tft.setCursor(100, 50);
                tft.print(buzzerTapScore);
                tone(BUZZER_PIN, TONE_FREQ * 2, 50); // Success sound
            } else {
                tone(BUZZER_PIN, TONE_FREQ / 2, 50); // Fail sound
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Constants for Time Challenge Game Progress Bar
static const int PROGRESS_BAR_X = 20;
static const int PROGRESS_BAR_Y = 180; // Below Timer and Diff
static const int PROGRESS_BAR_WIDTH = 200;
static const int PROGRESS_BAR_HEIGHT = 10;
static const uint16_t PROGRESS_BAR_COLOR = TFT_GREEN;
static const uint16_t PROGRESS_BAR_BG_COLOR = TFT_DARKGREY;


void TimeChallengeGame() {
    tft.fillScreen(TFT_BLACK);
    menuSprite.fillSprite(TFT_BLACK); // Clear the sprite before drawing game elements
    // Reusing global menuSprite for double buffering
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2); // Initial text size for Target and Timer labels

    unsigned long lastBuzzerTime = 0;
    const unsigned long BUZZER_INTERVAL_MS = 1000;

    unsigned long targetTimeMs = random(8000, 12001); // 10 to 20 seconds
    float targetTimeSec = targetTimeMs / 1000.0;

    menuSprite.setCursor(20, 30);
    menuSprite.printf("Target: %.1f s", targetTimeSec);

    menuSprite.setCursor(20, 80);
    menuSprite.print("Timer: 0.0 s"); // Timer label

    // Draw progress bar background
    menuSprite.drawRect(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, TFT_WHITE); // Border for the bar
    menuSprite.fillRect(PROGRESS_BAR_X + 1, PROGRESS_BAR_Y + 1, PROGRESS_BAR_WIDTH - 2, PROGRESS_BAR_HEIGHT - 2, PROGRESS_BAR_BG_COLOR); // Background fill

    unsigned long startTime = millis();
    unsigned long pressTime = 0;
    bool gameEnded = false;

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            return; // Exit game
        }
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        unsigned long currentTime = millis();

        if (!gameEnded) {
            float elapsedSec = (currentTime - startTime) / 1000.0;
            menuSprite.fillRect(100, 80, 120, 20, TFT_BLACK); // Clear old timer
            menuSprite.setTextSize(3); // Set larger size for timer number
            menuSprite.setCursor(100, 80);
            menuSprite.printf("%.1f s", elapsedSec); // <-- TIMER NUMBER
            menuSprite.setTextSize(2); // Reset to default size for labels

            // Update progress bar
            float progressRatio = (float)(currentTime - startTime) / targetTimeMs;
            int filledWidth = (int)(progressRatio * PROGRESS_BAR_WIDTH);
            
            // Ensure filledWidth does not exceed PROGRESS_BAR_WIDTH
            if (filledWidth > PROGRESS_BAR_WIDTH) {
                filledWidth = PROGRESS_BAR_WIDTH;
            }
            
            // Clear previous progress and draw new progress
            menuSprite.fillRect(PROGRESS_BAR_X + 1, PROGRESS_BAR_Y + 1, PROGRESS_BAR_WIDTH - 2, PROGRESS_BAR_HEIGHT - 2, PROGRESS_BAR_BG_COLOR); // Clear the filled part
            menuSprite.fillRect(PROGRESS_BAR_X + 1, PROGRESS_BAR_Y + 1, filledWidth, PROGRESS_BAR_HEIGHT - 2, PROGRESS_BAR_COLOR); // Draw new filled part

            
            // Ensure filledWidth does not exceed PROGRESS_BAR_WIDTH
            if (filledWidth > PROGRESS_BAR_WIDTH) {
                filledWidth = PROGRESS_BAR_WIDTH;
            }

            // Buzzer sound every second
            if (currentTime - lastBuzzerTime >= BUZZER_INTERVAL_MS) {
                tone(BUZZER_PIN, 1000, 50); // Play a tone (e.g., 1000 Hz for 50 ms)
                lastBuzzerTime = currentTime;
            }
            
        }
        menuSprite.pushSprite(0, 0); // Push the sprite to the screen

        if (readButtonLongPress()) { return; } // Exit game
        else if (readButton()) { // Handle short clicks for gameplay
            if (gamesDetectDoubleClick()) { // Double click detected
                //exitSubMenu = true; // Signal to exit
                return; // Exit game
            }
            if (!gameEnded) {
                pressTime = currentTime;
                gameEnded = true;
                tone(BUZZER_PIN, 1500, 100); // Confirmation sound
                
                float diffSec = (float)((long)(pressTime - startTime) - (long)targetTimeMs) / 1000.0;
                menuSprite.setTextSize(2);
                menuSprite.setCursor(20, 130);
                menuSprite.printf("Diff: %.2f s", diffSec);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


// --- Car Dodger Game Implementation ---
#define UPT_MOVE_NONE   0
#define UPT_MOVE_UP     1
#define UPT_MOVE_DOWN   2

#define CAR_COUNT       3
#define CAR_WIDTH       12
#define CAR_LENGTH      15
#define ROAD_SPEED      6

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t speed;
} s_otherCars;

typedef struct {
    bool hit;
    uint8_t lane;
    uint8_t y;
} s_myCar;

// Placeholder bitmaps (replace with actual ones if available in img.h or define new ones)
const uint16_t carImg_bitmap[] = {
    0x40,0xF8,0xEC,0x2C,0x2C,0x38,0xF0,0x10,0xD0,0x30,0xE8,0x4C,0x4C,0x9C,0xF0, // First 15 bytes
    0x02,0x1F,0x37,0x34,0x34,0x1C,0x0F,0x08,0x0B,0x0C,0x17,0x32,0x32,0x39,0x0F  // Next 15 bytes
}; // Assuming 15x16 pixels, 2 bytes per column for 16-bit color

const uint16_t roadMarking_bitmap[] = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
}; // Example 8x8 white block

static uint32_t highscore_carDodger; // Renamed to avoid conflict
static uint32_t score_carDodger;     // Renamed to avoid conflict
static uint8_t uptMove_carDodger;    // Renamed to avoid conflict
static uint8_t lives_carDodger;      // Renamed to avoid conflict
static s_otherCars cars[CAR_COUNT];

// Game draw function
void carDodger_draw() {
    static s_myCar myCar;
    static unsigned long hitTime;
    static bool newHighscore;

    // Change lane
    if (uptMove_carDodger == UPT_MOVE_UP && myCar.lane < 3)
        myCar.lane++;
    else if (uptMove_carDodger == UPT_MOVE_DOWN && myCar.lane > 0)
        myCar.lane--;
    uptMove_carDodger = UPT_MOVE_NONE;

    // Move to new lane
    uint8_t y = myCar.lane * 16;
    if (myCar.y > y)
        myCar.y -= 2;
    else if (myCar.y < y)
        myCar.y += 2;

    if (lives_carDodger != 255) {
        // Move cars
        for (uint8_t i = 0; i < CAR_COUNT; i++) {
            // Move car
            cars[i].x -= cars[i].speed;

            // Gone off screen
            if (cars[i].x > 200 && cars[i].x < 255 - 16) { // Original overflow check
                cars[i].x = SCREEN_WIDTH;
                cars[i].y = (random(0, 4)) * 16; // random(0,4) for 0,1,2,3
                score_carDodger++;
            }
        }

        // Stop cars from going through each other
        // Its a little bit glitchy
        for (uint8_t i = 0; i < CAR_COUNT; i++) {
            for (uint8_t c = 0; c < CAR_COUNT; c++) {
                if (i != c && cars[i].y == cars[c].y && cars[i].x > cars[c].x && cars[i].x < cars[c].x + CAR_LENGTH) {
                    cars[i].x = cars[c].x + CAR_LENGTH + 1;
                    //cars[i].speed = cars[c].speed;
                }
            }
        }

        // Collision
        if (!myCar.hit) {
            for (uint8_t i = 0; i < CAR_COUNT; i++) {
                if (cars[i].x < CAR_LENGTH) {
                    uint8_t carY = cars[i].y + 2;
                    uint8_t myCarY = myCar.y + 2;
                    if ((carY >= myCarY && carY <= myCarY + CAR_WIDTH) || (carY + CAR_WIDTH >= myCarY && carY + CAR_WIDTH <= myCarY + CAR_WIDTH)) {
                        myCar.hit = true;
                        hitTime = millis();
                        lives_carDodger--;
                        if (lives_carDodger == 255) {
                            // Check for new highscore
                            if (score_carDodger > highscore_carDodger) {
                                newHighscore = true;
                                highscore_carDodger = score_carDodger;
                                // eeprom_update_block(&highscore, (uint*)&eepHighscore, sizeof(uint)); // Removed
                            } else {
                                newHighscore = false;
                            }
                            // led_flash(LED_RED, 250, 255); // Replaced with direct LED toggle
                            tone(BUZZER_PIN, 2000, 250); // Equivalent to buzzer_buzz(250, TONE_2KHZ, ...)
                        } else {
                            // led_flash(LED_RED, 30, 255); // Replaced with direct LED toggle
                            tone(BUZZER_PIN, 2000, 100); // Equivalent to buzzer_buzz(100, TONE_2KHZ, ...)
                        }
                    }
                }
            }
        }
    }

    unsigned long now = millis();

    if (myCar.hit && now - hitTime >= 1000)
        myCar.hit = false;

    // Quake
    static int8_t quakeY;
    if (myCar.hit && now - hitTime < 350) {
        if (quakeY == 2)
            quakeY = -2;
        else
            quakeY = 2;
    } else {
        quakeY = 0;
    }

    // Draw my car
    if (!myCar.hit || (myCar.hit && (now & 64)))
        draw_game_bitmap(0, myCar.y + quakeY, carImg_bitmap, 15, 16);

    char buff[10]; // Increased buffer size
    if (lives_carDodger != 255) {
        // Draw other cars
        for (int8_t i = CAR_COUNT - 1; i >= 0; i--) // LOOPR
            draw_game_bitmap(cars[i].x, cars[i].y + quakeY, carImg_bitmap, 15, 16);

        // Draw road markings
        static uint8_t dotX[3] = {0, 45, 90};
        for (uint8_t i = 0; i < 3; i++) {
            dotX[i] -= ROAD_SPEED;

            if (dotX[i] >= SCREEN_WIDTH && dotX[i] < 255 - 8) // Original overflow check
                dotX[i] = SCREEN_WIDTH;

            for (uint8_t x = 0; x < 3; x++)
                draw_game_bitmap(dotX[i], (x * 16) + quakeY + 16, roadMarking_bitmap, 8, 8);
        }

        // Draw score
        sprintf(buff, "%lu", score_carDodger);
        menuSprite.setTextSize(1); // Set text size for score
        menuSprite.setTextDatum(TR_DATUM); // Align to top right
        menuSprite.drawString(buff, SCREEN_WIDTH - 5, 1); // Adjusted position
        menuSprite.setTextDatum(TL_DATUM); // Reset to top left

        // Draw lives
        for (uint8_t i = 0; i < lives_carDodger; i++)
            draw_game_bitmap(32 + (8 * i), 1, bird_big, 7, 8); // Using bird_big as placeholder for livesImg
    } else {
        // Draw end game stuff
        menuSprite.setTextSize(2);
        menuSprite.drawString("GAME OVER", 20, 0);
        menuSprite.drawString("SCORE:", 20, 16);
        menuSprite.drawString("HIGHSCORE:", 20, 32);

        if (newHighscore)
            menuSprite.drawString("NEW HIGHSCORE!", 20, 48);

        sprintf(buff, "%lu", score_carDodger);
        menuSprite.drawString(buff, 96, 16);

        sprintf(buff, "%lu", highscore_carDodger);
        menuSprite.drawString(buff, 96, 32);
    }

    menuSprite.pushSprite(0, 0); // Push to screen
}

void carDodgerGame() {
    tft.fillScreen(TFT_BLACK); // Clear screen

    // Initialize game variables
    for (uint8_t i = 0; i < CAR_COUNT; i++) {
        cars[i].y = i * 16;
        cars[i].speed = i + 1;
        cars[i].x = SCREEN_WIDTH;
    }

    score_carDodger = 0;
    uptMove_carDodger = UPT_MOVE_NONE;
    lives_carDodger = 3;
    highscore_carDodger = 0; // Initialize highscore

    // Game loop
    while (true) {
        if (exitSubMenu || g_alarm_is_ringing) {
            return; // Exit game
        }

        // Button handling
        if (readButton()) {
            if (gamesDetectDoubleClick()) {
                return; // Exit game on double click
            }
            // Single click for lane change
            if (uptMove_carDodger == UPT_MOVE_NONE) { // Only allow move if not already moving
                if (readEncoder() == 1) { // Right encoder turn for UP
                    uptMove_carDodger = UPT_MOVE_UP;
                } else if (readEncoder() == -1) { // Left encoder turn for DOWN
                    uptMove_carDodger = UPT_MOVE_DOWN;
                }
            }
        }

        carDodger_draw(); // Draw current frame
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay for frame rate
    }
}
// --- Flappy Bird Game Implementation ---
// Based on game/main.c

// Game constants
#define BIRD_X 40
#define BIRD_RADIUS 5
#define GRAVITY 0.3
#define JUMP_FORCE -4.5
#define PIPE_WIDTH 20
#define PIPE_GAP 80
#define PIPE_SPEED 2
#define PIPE_INTERVAL 120 // pixels between pipes

void flappy_bird_game() {
    // Game variables
    float bird_y = SCREEN_HEIGHT / 2;
    float bird_vy = 0;
    int pipes_x[2];
    int pipes_y[2];
    int score = 0;
    bool game_over = false;
    bool start_game = false;

    // Initialize pipes
    pipes_x[0] = SCREEN_WIDTH;
    pipes_y[0] = random(40, SCREEN_HEIGHT - 40 - PIPE_GAP);
    pipes_x[1] = SCREEN_WIDTH + PIPE_INTERVAL + (PIPE_WIDTH / 2);
    pipes_y[1] = random(40, SCREEN_HEIGHT - 40 - PIPE_GAP);

    unsigned long lastFrameTime = millis();
    unsigned long lastClickTime = 0;

    while (true) {
        // --- Exit Condition ---
        if (exitSubMenu) { // ADD THIS LINE
            exitSubMenu = false; // Reset flag
            return; // Exit game
        }
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        if (readButtonLongPress()) { return; }

        // ADD THIS BLOCK
        if (readButton()) {
            if (gamesDetectDoubleClick()) { // Double click detected
               // exitSubMenu = true; // Signal to exit
                return; // Exit game
            }
            lastClickTime = millis(); // For single click game logic
        }

        // --- Logic ---
        if (!start_game) {
            if (millis() - lastClickTime < 500 && millis() - lastClickTime > 0) {
                start_game = true;
                lastClickTime = 0;
            }
        } else if (!game_over) {
            // Frame Rate Control
            unsigned long currentTime = millis();
            if (currentTime - lastFrameTime < 30) { // ~33 FPS
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
            lastFrameTime = currentTime;

            // Input
            if (millis() - lastClickTime < 500 && millis() - lastClickTime > 0) {
                bird_vy = JUMP_FORCE;
                tone(BUZZER_PIN, 1500, 20);
                lastClickTime = 0;
            }

            // Bird Physics
            bird_vy += GRAVITY;
            bird_y += bird_vy;

            // Pipe Logic
            for (int i = 0; i < 2; i++) {
                pipes_x[i] -= PIPE_SPEED;
                if (pipes_x[i] < -PIPE_WIDTH) {
                    pipes_x[i] = SCREEN_WIDTH;
                    pipes_y[i] = random(40, SCREEN_HEIGHT - 40 - PIPE_GAP);
                }
                if (pipes_x[i] + PIPE_WIDTH < BIRD_X && pipes_x[i] + PIPE_WIDTH + PIPE_SPEED >= BIRD_X) {
                    score++;
                    tone(BUZZER_PIN, 2500, 20);
                }
            }

            // Collision Detection
            if (bird_y + BIRD_RADIUS > SCREEN_HEIGHT || bird_y - BIRD_RADIUS < 0) {
                game_over = true;
            }
            for (int i = 0; i < 2; i++) {
                if (BIRD_X + BIRD_RADIUS > pipes_x[i] && BIRD_X - BIRD_RADIUS < pipes_x[i] + PIPE_WIDTH) {
                    if (bird_y - BIRD_RADIUS < pipes_y[i] || bird_y + BIRD_RADIUS > pipes_y[i] + PIPE_GAP) {
                        game_over = true;
                    }
                }
            }
            if (game_over) {
                tone(BUZZER_PIN, 500, 200);
            }
        } else { // Game is over
            if (millis() - lastClickTime < 500 && millis() - lastClickTime > 0) {
                // Reset game
                bird_y = SCREEN_HEIGHT / 2;
                bird_vy = 0;
                score = 0;
                pipes_x[0] = SCREEN_WIDTH;
                pipes_y[0] = random(40, SCREEN_HEIGHT - 40 - PIPE_GAP);
                pipes_x[1] = SCREEN_WIDTH + PIPE_INTERVAL + (PIPE_WIDTH / 2);
                pipes_y[1] = random(40, SCREEN_HEIGHT - 40 - PIPE_GAP);
                game_over = false;
                start_game = false;
                lastClickTime = 0;
            }
        }

        // --- Drawing (using menuSprite) ---
        menuSprite.fillSprite(TFT_BLACK); // Clear buffer

        if (!start_game) {
            menuSprite.setTextSize(2);
            menuSprite.setCursor(50, SCREEN_HEIGHT / 2 + 10);
            menuSprite.print("Click to start");
        } else {
            // Draw Bird
            menuSprite.pushImage(BIRD_X - 5, (int)bird_y - 4, 11, 8, bird);

            // Draw Pipes
            for (int i = 0; i < 2; i++) {
                menuSprite.fillRect(pipes_x[i], 0, PIPE_WIDTH, pipes_y[i], TFT_GREEN);
                menuSprite.fillRect(pipes_x[i] - 2, pipes_y[i] - 10, PIPE_WIDTH + 4, 10, TFT_GREEN);
                menuSprite.fillRect(pipes_x[i], pipes_y[i] + PIPE_GAP, PIPE_WIDTH, SCREEN_HEIGHT - (pipes_y[i] + PIPE_GAP), TFT_GREEN);
                menuSprite.fillRect(pipes_x[i] - 2, pipes_y[i] + PIPE_GAP, PIPE_WIDTH + 4, 10, TFT_GREEN);
            }

            // Draw Score
            menuSprite.setTextSize(2);
            menuSprite.setCursor(10, 10);
            menuSprite.printf("Score: %d", score);

            if (game_over) {
                menuSprite.setTextSize(3);
                menuSprite.setCursor(40, SCREEN_HEIGHT / 2 - 30);
                menuSprite.print("Game Over");
                menuSprite.setTextSize(2);
                menuSprite.setCursor(50, SCREEN_HEIGHT / 2 + 10);
                menuSprite.print("Click to restart");
            }
        }

        // Common text
        menuSprite.setTextSize(1);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setCursor(20, 220);
        menuSprite.print("Click to jump, Double-click to exit");

        menuSprite.pushSprite(0, 0); // Push buffer to screen

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

