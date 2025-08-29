#include "RotaryEncoder.h"
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "img.h"
#include "LED.h"
#include "Buzzer.h"
#include "weather.h"
#include "performance.h"
#include "DS18B20.h"
#include "animation.h"
#include "Games.h"
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
    // {"Snake Game", snake, tanchisheGame},
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
void initSnakeGame();
void drawSnake();
void generateFood();
void updateSnake();
void handleSnakeInput();
void drawGameOver();

void tanchisheGame() {
  initSnakeGame(); // Initialize game state

  tft.fillScreen(TFT_BLACK); // Clear screen
  tft.drawRect(SNAKE_START_X - 1, SNAKE_START_Y - 1, SNAKE_GRID_WIDTH * SNAKE_CELL_SIZE + 2, SNAKE_GRID_HEIGHT * SNAKE_CELL_SIZE + 2, TFT_WHITE); // Draw border
  drawSnake(); // Initial draw of the snake and food

  unsigned long lastGameUpdateTime = millis();

  while (true) {
    // Input handling
    handleSnakeInput();

    // Game update logic
    unsigned long currentTime = millis();
    if (!gameOver && (currentTime - lastGameUpdateTime > SNAKE_GAME_SPEED_MS)) {
      updateSnake();
      drawSnake(); // Redraw only after snake state updates
      lastGameUpdateTime = currentTime;
    }

    // Game over check and display
    if (gameOver) {
      drawGameOver();
    }

    // Exit condition (double click)
    if (readButton()) {
      if (gamesDetectDoubleClick()) {
        return; // Exit game
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay for responsiveness
  }
}

void initSnakeGame() {
  snakeBody.clear();
  for (int i = 0; i < SNAKE_INITIAL_LENGTH; ++i) {
    snakeBody.push_back({SNAKE_GRID_WIDTH / 2, SNAKE_GRID_HEIGHT / 2 + i}); // Start in middle, moving up
  }
  currentDirection = SNAKE_UP;
  gameOver = false;
  snakeScore = 0;
  generateFood();
}

void drawSnake() {
  // This version clears the play area first, then redraws everything.
  // This is the simplest way to fix the "trail" bug.
  tft.fillRect(SNAKE_START_X, SNAKE_START_Y, SNAKE_GRID_WIDTH * SNAKE_CELL_SIZE, SNAKE_GRID_HEIGHT * SNAKE_CELL_SIZE, TFT_BLACK);

  // Draw food (always redraw to ensure it's visible)
  tft.fillRect(SNAKE_START_X + food.x * SNAKE_CELL_SIZE, SNAKE_START_Y + food.y * SNAKE_CELL_SIZE, SNAKE_CELL_SIZE, SNAKE_CELL_SIZE, TFT_RED);

  // Draw the entire snake body
  for (const auto& segment : snakeBody) {
    tft.fillRect(SNAKE_START_X + segment.x * SNAKE_CELL_SIZE, SNAKE_START_Y + segment.y * SNAKE_CELL_SIZE, SNAKE_CELL_SIZE, SNAKE_CELL_SIZE, TFT_GREEN);
  }

  // Draw score
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.printf("Score: %d", snakeScore);
}

void generateFood() {
  bool foodOnSnake;
  do {
    food.x = random(SNAKE_GRID_WIDTH);
    food.y = random(SNAKE_GRID_HEIGHT);
    foodOnSnake = false;
    for (const auto& segment : snakeBody) {
      if (segment.x == food.x && segment.y == food.y) {
        foodOnSnake = true;
        break;
      }
    }
  } while (foodOnSnake);
}

void updateSnake() {
  // Move head
  Point newHead = snakeBody.front();
  switch (currentDirection) {
    case SNAKE_UP:    newHead.y--; break;
    case SNAKE_DOWN:  newHead.y++; break;
    case SNAKE_LEFT:  newHead.x--; break;
    case SNAKE_RIGHT: newHead.x++; break;
  }

  // Collision with walls
  if (newHead.x < 0 || newHead.x >= SNAKE_GRID_WIDTH ||
      newHead.y < 0 || newHead.y >= SNAKE_GRID_HEIGHT) {
    gameOver = true;
    return;
  }

  // Collision with self (start from index 1 to avoid checking against the current head)
  for (size_t i = 1; i < snakeBody.size(); ++i) {
    if (newHead.x == snakeBody[i].x && newHead.y == snakeBody[i].y) {
      gameOver = true;
      return;
    }
  }

  snakeBody.insert(snakeBody.begin(), newHead); // Add new head

  // Check if food is eaten
  if (newHead.x == food.x && newHead.y == food.y) {
    snakeScore++;
    generateFood();
  } else {
    snakeBody.pop_back(); // Remove tail if no food eaten
  }
}

void handleSnakeInput() {
  int encoderDelta = readEncoder();
  if (encoderDelta != 0) {
    if (encoderDelta == 1) { // Clockwise rotation
      switch (currentDirection) {
        case SNAKE_UP:    currentDirection = SNAKE_RIGHT; break;
        case SNAKE_RIGHT: currentDirection = SNAKE_DOWN;  break;
        case SNAKE_DOWN:  currentDirection = SNAKE_LEFT;  break;
        case SNAKE_LEFT:  currentDirection = SNAKE_UP;    break;
      }
    } else if (encoderDelta == -1) { // Counter-clockwise rotation
      switch (currentDirection) {
        case SNAKE_UP:    currentDirection = SNAKE_LEFT;  break;
        case SNAKE_LEFT:  currentDirection = SNAKE_DOWN;  break;
        case SNAKE_DOWN:  currentDirection = SNAKE_RIGHT; break;
        case SNAKE_RIGHT: currentDirection = SNAKE_UP;    break;
      }
    }
  }
}

void drawGameOver() {
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(SCREEN_WIDTH / 2 - tft.textWidth("GAME OVER") / 2, SCREEN_HEIGHT / 2 - 10);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setCursor(SCREEN_WIDTH / 2 - tft.textWidth("Score: ") / 2, SCREEN_HEIGHT / 2 + 10);
  tft.printf("Score: %d", snakeScore);
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

        if (readButton()) { // Use readButton() directly
            if (gamesDetectDoubleClick()) { // Double click detected
                return; // Exit game
            } else { // Single click
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
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Dino Game Implementation ---
#define DINO_GROUND_Y (SCREEN_HEIGHT - 40)
#define DINO_X 40
#define DINO_WIDTH 20
#define DINO_HEIGHT 20
#define JUMP_VELOCITY -6
#define GRAVITY 0.8

struct Obstacle {
    int x;
    int width;
    int height;
};

void dinoGame() {
    tft.fillScreen(TFT_BLACK);

    float dino_y = DINO_GROUND_Y - DINO_HEIGHT;
    float dino_vy = 0;
    bool dino_is_jumping = false;
    int dino_score = 0;
    bool dino_game_over = false;
    float game_speed = 4.0;

    std::vector<Obstacle> obstacles;
    unsigned long last_obstacle_time = 0;

    while (true) {
        // --- Input ---
        if (readButton() && !dino_is_jumping) {
            dino_is_jumping = true;
            dino_vy = JUMP_VELOCITY;
            tone(BUZZER_PIN, 1500, 50);
        }
        
        if (readButton()) {
            if (gamesDetectDoubleClick()) {
                return; // Exit game
            }
        }

        if (!dino_game_over) {
            // --- Update ---
            // Dino physics
            if (dino_is_jumping) {
                dino_y += dino_vy;
                dino_vy += GRAVITY;
                if (dino_y >= DINO_GROUND_Y - DINO_HEIGHT) {
                    dino_y = DINO_GROUND_Y - DINO_HEIGHT;
                    dino_is_jumping = false;
                    dino_vy = 0;
                }
            }

            // Obstacles
            if (millis() - last_obstacle_time > random(1000, 3000) / (game_speed / 4.0)) {
                obstacles.push_back({SCREEN_WIDTH, 15, 30});
                last_obstacle_time = millis();
            }

            for (auto &obs : obstacles) {
                obs.x -= game_speed;
            }

            // Remove off-screen obstacles and increase score
            for (int i = 0; i < obstacles.size(); ++i) {
                if (obstacles[i].x < -obstacles[i].width) {
                    obstacles.erase(obstacles.begin() + i);
                    dino_score++;
                    game_speed += 0.1; // Increase speed
                    i--;
                }
            }

            // Collision detection
            for (const auto &obs : obstacles) {
                if (DINO_X + DINO_WIDTH > obs.x && DINO_X < obs.x + obs.width &&
                    dino_y + DINO_HEIGHT > DINO_GROUND_Y - obs.height) {
                    dino_game_over = true;
                    tone(BUZZER_PIN, 500, 200);
                }
            }

        } else {
            // Game over state
            if (readButton()) { // Press button to restart
                dino_y = DINO_GROUND_Y - DINO_HEIGHT;
                dino_vy = 0;
                dino_is_jumping = false;
                dino_score = 0;
                dino_game_over = false;
                game_speed = 4.0;
                obstacles.clear();
                last_obstacle_time = millis();
            }
        }

        // --- Draw ---
        tft.fillScreen(TFT_BLACK);

        // Draw Ground
        tft.drawLine(0, DINO_GROUND_Y, SCREEN_WIDTH, DINO_GROUND_Y, TFT_WHITE);

        // Draw Dino
        tft.fillRect(DINO_X, dino_y, DINO_WIDTH, DINO_HEIGHT, TFT_GREEN);

        // Draw Obstacles
        for (const auto &obs : obstacles) {
            tft.fillRect(obs.x, DINO_GROUND_Y - obs.height, obs.width, obs.height, TFT_RED);
        }

        // Draw Score
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(10, 10);
        tft.printf("Score: %d", dino_score);

        if (dino_game_over) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(SCREEN_WIDTH / 2 - tft.textWidth("GAME OVER") / 2, SCREEN_HEIGHT / 2 - 10);
            tft.print("GAME OVER");
            tft.setTextSize(1);
            tft.setCursor(SCREEN_WIDTH / 2 - tft.textWidth("Press to restart") / 2, SCREEN_HEIGHT / 2 + 20);
            tft.print("Press to restart");
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
