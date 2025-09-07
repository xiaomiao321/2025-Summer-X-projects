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
void tanchisheGame();
void ClickerGame();
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
    {"Conway's Game", bird_big, ConwayGame},
    // {"Snake Game", snake, tanchisheGame},
    {"Buzzer Tap", bird_big, BuzzerTapGame},
    {"Time Challenge", Timer, TimeChallengeGame},
    {"Flappy Bird", bird_big, flappy_bird_game}, // Using Dinasor icon as placeholder
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

