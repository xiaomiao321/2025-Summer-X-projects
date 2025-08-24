#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "img.h"
#include "LED.h"
#include "Buzzer.h"
#include "weather.h"
#include "performance.h"
#include "DS18B20.h"
#include "animation.h"
#include "Games.h"

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
bool gamesDetectDoubleClick() {
  static unsigned long lastClickTime = 0;
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
    {"Snake Game", snake, tanchisheGame},
    {"Buzzer Tap", Sound, BuzzerTapGame},
    {"Time Challenge", Conway, TimeChallengeGame},
};
const uint8_t GAME_ITEM_COUNT = sizeof(gameItems) / sizeof(gameItems[0]);

// --- Drawing Functions ---
void drawGameIcons(int16_t offset) {
    menuSprite.fillSprite(TFT_BLACK);

    // Selection triangle indicator
    int16_t triangle_x = offset + (game_picture_flag * ICON_SPACING) + (ICON_SIZE / 2);
    menuSprite.fillTriangle(triangle_x, TRIANGLE_BASE_Y, triangle_x - 12, TRIANGLE_PEAK_Y, triangle_x + 12, TRIANGLE_PEAK_Y, TFT_WHITE);

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
    menuSprite.drawString("Select", 10, 220);

    menuSprite.pushSprite(0, 0);
}

// --- Main Game Menu Function ---
void GamesMenu() {
    animateMenuTransition("GAMES", true);

    // Reset state for the game menu
    game_picture_flag = 0;
    game_display = INITIAL_X_OFFSET;
    drawGameIcons(game_display);

    // State for delayed single-click in GamesMenu
    static unsigned long gamesMenuLastClickTime = 0;
    static bool gamesMenuSingleClickPending = false;

    while (true) {
        int direction = readEncoder();
        if (direction != 0) {
            if (direction == 1) {
                game_picture_flag = (game_picture_flag + 1) % GAME_ITEM_COUNT;
            } else if (direction == -1) {
                game_picture_flag = (game_picture_flag == 0) ? GAME_ITEM_COUNT - 1 : game_picture_flag - 1;
            }
            tone(BUZZER_PIN, 1000 * (game_picture_flag + 1), 20);
            int16_t target_display = INITIAL_X_OFFSET - (game_picture_flag * ICON_SPACING);

            for (uint8_t step = ANIMATION_STEPS; step > 0; step--) {
                ui_run_easing(&game_display, target_display, step);
                drawGameIcons(game_display);
                vTaskDelay(pdMS_TO_TICKS(15));
            }

            game_display = target_display;
            drawGameIcons(game_display);
        }

        if (readButton()) { // Use readButton() directly
            if (gamesDetectDoubleClick()) { // Double click detected
                gamesMenuSingleClickPending = false; // Cancel any pending single click
                animateMenuTransition("GAMES", false);
                display = 48; // Restore main menu state as in other files
                picture_flag = 0;
                showMenuConfig();
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
            animateMenuTransition("GAMES", true);
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
        unsigned long currentTime = millis();

        // Auto-run logic
        if (currentTime - lastUpdateTime > UPDATE_INTERVAL) {
            updateConwayGrid();
            drawConwayGrid();
            lastUpdateTime = currentTime;
        }

        // Button handling for exit
        if (readButton()) { // Use readButton() directly
            if (gamesDetectDoubleClick()) { // Double click detected
                return; // Exit
            } else {
                // Single click has no effect in auto-run mode
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay for responsiveness
    }
}

// --- Placeholder for Snake Game ---
void tanchisheGame() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(40, 100);
    tft.print("Snake Game");
    tft.setTextSize(1);
    tft.setCursor(35, 140);
    tft.print("Double-click to exit");

    while (true) {
        if (readButton()) { // Use readButton() directly
            if (gamesDetectDoubleClick()) { // Double click detected
                return; // Exit tanchisheGame
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
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
        unsigned long currentTime = millis();

        // Play tone
        if (currentTime - lastToneTime > nextToneInterval) {
            tone(BUZZER_PIN, TONE_FREQ, TONE_DURATION);
            lastToneTime = currentTime;
            nextToneInterval = random(1000, 3000); // New random interval
        }

        if (readButton()) { // Use readButton() directly
            if (gamesDetectDoubleClick()) {
                noTone(BUZZER_PIN); // Stop any ongoing tone
                return; // Exit
            } else {
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
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Time Challenge Game Implementation ---
void TimeChallengeGame() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    unsigned long targetTimeMs = random(8000, 12001); // 10 to 20 seconds
    float targetTimeSec = targetTimeMs / 1000.0;

    tft.setCursor(20, 30);
    tft.printf("Target: %.1f s", targetTimeSec);

    tft.setCursor(20, 80);
    tft.print("Timer: 0.0 s");

    tft.setTextSize(1);
    tft.setCursor(35, 180);
    tft.print("Press at target, Double-click to exit");

    unsigned long startTime = millis();
    unsigned long pressTime = 0;
    bool gameEnded = false;

    while (true) {
        unsigned long currentTime = millis();

        if (!gameEnded) {
            float elapsedSec = (currentTime - startTime) / 1000.0;
            tft.fillRect(100, 80, 120, 20, TFT_BLACK); // Clear old timer
            tft.setCursor(100, 80);
            tft.printf("%.1f s", elapsedSec);
        }

        if (readButton()) { // Use readButton() directly
            if (gamesDetectDoubleClick()) { // Double click detected
                return; // Exit game
            } else { // Single click
                if (!gameEnded) {
                    pressTime = currentTime;
                    gameEnded = true;
                    tone(BUZZER_PIN, 1500, 100); // Confirmation sound
                    
                    float diffSec = (pressTime - startTime - targetTimeMs) / 1000.0;
                    tft.setTextSize(2);
                    tft.setCursor(20, 130);
                    tft.printf("Diff: %.2f s", diffSec);
                    tft.setTextSize(1); // Reset text size
                    tft.setCursor(35, 200);
                    tft.print("Double-click to exit");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}