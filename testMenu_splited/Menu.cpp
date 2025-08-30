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
#include "ADC.h"
#include "Watchface.h" // <-- ADDED

// --- Layout Configuration ---
// Change these values to adjust the menu layout
static const int ICON_SIZE = 200;     // The size for the icons (e.g., 180x180)
static const int ICON_SPACING = 220;  // The horizontal space between icons (should be > ICON_SIZE)
static const int SCREEN_WIDTH = 240;
static const int SCREEN_HEIGHT = 240;

// Calculated layout values
static const int ICON_Y_POS = (SCREEN_HEIGHT / 2) - (ICON_SIZE / 2); // Center the icon vertically

static const int INITIAL_X_OFFSET = (SCREEN_WIDTH / 2) - (ICON_SIZE / 2); // Center the first icon

static const int TRIANGLE_BASE_Y = ICON_Y_POS - 5;
static const int TRIANGLE_PEAK_Y = TRIANGLE_BASE_Y - 20;

int16_t display = INITIAL_X_OFFSET; // Icon initial x-offset
uint8_t picture_flag = 0;           // Current selected menu item index

// Menu item structure
struct MenuItem {
    const char *name;              // Menu item name
    const uint16_t *image;         // Menu item image
};

// Menu items array
const MenuItem menuItems[] = {
    {"Clock", Weather},
    {"Countdown", Countdown}, // Placeholder image
    {"Stopwatch", Timer},   // Changed placeholder image
    {"Music", Music},
    {"Performance", Performance},
    {"Temperature",Temperature},
    {"Animation",Animation},
    {"Games", Games},
    {"LED", LED},
    {"ADC", ADC},      // Added ADC Voltmeter
};
const uint8_t MENU_ITEM_COUNT = sizeof(menuItems) / sizeof(menuItems[0]); // Number of menu items

// Menu state enum
enum MenuState {
    MAIN_MENU,
    SUB_MENU,
    ANIMATING
};

// Global state variables
static MenuState current_state = MAIN_MENU;
static const uint8_t ANIMATION_STEPS = 12;

// Easing function
float easeOutQuad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}



// Draw main menu
void drawMenuIcons(int16_t offset) {
    // Clear the region where icons and triangle are drawn
    menuSprite.fillRect(0, ICON_Y_POS, SCREEN_WIDTH, SCREEN_HEIGHT - ICON_Y_POS, TFT_BLACK);

    // Clear the text area at the top
    menuSprite.fillRect(0, 0, SCREEN_WIDTH, 40, TFT_BLACK); // Clear from y=0 to y=40

    // Selection triangle indicator
    int16_t triangle_x = offset + (picture_flag * ICON_SPACING) + (ICON_SIZE / 2);
    menuSprite.fillTriangle(triangle_x, SCREEN_HEIGHT - 25, triangle_x - 12, SCREEN_HEIGHT - 5, triangle_x + 12, SCREEN_HEIGHT - 5, TFT_WHITE);

    // Icons
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int16_t x = offset + (i * ICON_SPACING);
        if (x >= -ICON_SIZE && x < SCREEN_WIDTH) {
            menuSprite.pushImage(x, ICON_Y_POS, ICON_SIZE, ICON_SIZE, menuItems[i].image);
        }
    }

    // Text
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2); // Enlarge text
    menuSprite.setTextDatum(TC_DATUM); // Center text horizontally
    menuSprite.drawString(menuItems[picture_flag].name, SCREEN_WIDTH / 2, 10); // Centered menu item name
    

    menuSprite.pushSprite(0, 0);
}

// Show main menu
void showMenuConfig() {
    tft.fillScreen(TFT_BLACK);
    drawMenuIcons(display);
}



// Main menu navigation
void showMenu() {
    if (current_state != MAIN_MENU) return;
    
    int direction = readEncoder();
    if (direction != 0) {
        current_state = ANIMATING;
        
        if (direction == 1) { // Right
            picture_flag = (picture_flag + 1) % MENU_ITEM_COUNT;
        } else if (direction == -1) { // Left
            picture_flag = (picture_flag == 0) ? MENU_ITEM_COUNT - 1 : picture_flag - 1;
        }
        tone(BUZZER_PIN, 1000*(picture_flag + 1), 20);
        int16_t start_display = display; // Capture the starting position
        int16_t target_display = INITIAL_X_OFFSET - (picture_flag * ICON_SPACING);
        
        for (uint8_t i = 0; i <= ANIMATION_STEPS; i++) { // Loop from 0 to ANIMATION_STEPS
            float t = (float)i / ANIMATION_STEPS; // Progress from 0.0 to 1.0
            float eased_t = easeOutQuad(t); // Apply easing

            display = start_display + (target_display - start_display) * eased_t; // Calculate interpolated position

            drawMenuIcons(display);
            vTaskDelay(pdMS_TO_TICKS(20)); // Increased delay for smoother animation
        }
        
        display = target_display;
        drawMenuIcons(display);
        current_state = MAIN_MENU;
    }
    
    if (readButton()) {
        // Play confirm sound on selection
        tone(BUZZER_PIN, 2000, 50);
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to let the sound play

        switch (picture_flag) {
            case 0: weatherMenu(); showMenuConfig(); break;
            case 1: CountdownMenu(); showMenuConfig(); break; // Changed function name
            case 2: StopwatchMenu(); showMenuConfig(); break; // Changed function name
            case 3: BuzzerMenu(); showMenuConfig(); break;
            case 4: performanceMenu(); showMenuConfig(); break;
            case 5: DS18B20Menu(); showMenuConfig(); break;
            case 6: AnimationMenu(); showMenuConfig(); break;
            case 7: GamesMenu(); showMenuConfig(); break;
            case 8: LEDMenu(); showMenuConfig(); break;
            case 9: ADCMenu(); showMenuConfig(); break;
        }
    }
}