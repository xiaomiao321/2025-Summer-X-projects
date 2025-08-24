#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "img.h"
#include "LED.h"
#include "Buzzer.h"
#include "weather.h"
#include "performance.h"
#include "DS18B20.h"
#include "animation.h"

// --- Layout Configuration ---
// Change these values to adjust the menu layout
const int ICON_SIZE = 180;     // The size for the icons (e.g., 180x180)
const int ICON_SPACING = 200;  // The horizontal space between icons (should be > ICON_SIZE)
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 240;

// Calculated layout values
const int ICON_Y_POS = (SCREEN_HEIGHT / 2) - (ICON_SIZE / 2); // Center the icon vertically
const int TRIANGLE_BASE_Y = ICON_Y_POS - 5;                   // Triangle base sits just above the icon
const int TRIANGLE_PEAK_Y = TRIANGLE_BASE_Y - 20;             // Triangle peak is 20px higher
const int INITIAL_X_OFFSET = (SCREEN_WIDTH / 2) - (ICON_SIZE / 2); // Center the first icon

int16_t display = INITIAL_X_OFFSET; // Icon initial x-offset
uint8_t picture_flag = 0;           // Current selected menu item index

// Menu item structure
struct MenuItem {
    const char *name;              // Menu item name
    const uint16_t *image;         // Menu item image
};

// Menu items array
const MenuItem menuItems[] = {
    {"Music", Music},
    {"Weather", Weather},
    {"Performance", performance},
    {"LED",LED},
    {"Temperature",Temperature},
    {"Animation",performance}
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

// Smooth animation movement
void ui_run_easing(int16_t *current, int16_t target, uint8_t steps) {
    if (*current == target) return;
    float t = (float)(ANIMATION_STEPS - steps) / ANIMATION_STEPS;
    float eased = easeOutQuad(t);
    int16_t delta = target - *current;
    *current += (int16_t)(delta * eased / steps);
    if (abs(*current - target) < 2) {
        *current = target;
    }
}

// Draw main menu
void drawMenuIcons(int16_t offset) {
    menuSprite.fillSprite(TFT_BLACK);

    // Selection triangle indicator
    int16_t triangle_x = offset + (picture_flag * ICON_SPACING) + (ICON_SIZE / 2);
    menuSprite.fillTriangle(triangle_x, TRIANGLE_BASE_Y, triangle_x - 12, TRIANGLE_PEAK_Y, triangle_x + 12, TRIANGLE_PEAK_Y, TFT_WHITE);

    // Icons
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int16_t x = offset + (i * ICON_SPACING);
        if (x >= -ICON_SIZE && x < SCREEN_WIDTH) {
            menuSprite.pushImage(x, ICON_Y_POS, ICON_SIZE, ICON_SIZE, menuItems[i].image);
        }
    }

    // Text
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(1);
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.drawString("MENU:", 10, 10);
    menuSprite.drawString(menuItems[picture_flag].name, 50, 10);
    menuSprite.drawString("Select", 10, 220); // Bottom hint

    menuSprite.pushSprite(0, 0);
}

// Show main menu
void showMenuConfig() {
    tft.fillScreen(TFT_BLACK);
    drawMenuIcons(display);
}

// Animate transition (entering or exiting submenu)
void animateMenuTransition(const char *title, bool entering) {
    current_state = ANIMATING;
    
    int16_t start_y = entering ? 10 : 220;
    int16_t target_y = entering ? 220 : 10;
    int16_t animated_text_y = start_y;

    for (uint8_t step = ANIMATION_STEPS; step > 0; step--) {
        // Clear the sprite, not the screen
        menuSprite.fillSprite(TFT_BLACK);

        // Draw all elements to the sprite
        // Icons
        for (int i = 0; i < MENU_ITEM_COUNT; i++) {
            int16_t x = display + (i * ICON_SPACING);
            if (x >= -ICON_SIZE && x < SCREEN_WIDTH) {
                menuSprite.pushImage(x, ICON_Y_POS, ICON_SIZE, ICON_SIZE, menuItems[i].image);
            }
        }

        // Triangle
        int16_t triangle_x = display + (picture_flag * ICON_SPACING) + ICON_SIZE / 2;
        menuSprite.fillTriangle(triangle_x, TRIANGLE_BASE_Y, triangle_x - 12, TRIANGLE_PEAK_Y, triangle_x + 12, TRIANGLE_PEAK_Y, TFT_WHITE);

        // Animated text
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextSize(1);
        menuSprite.setTextDatum(TL_DATUM);
        menuSprite.drawString("MENU:", 10, animated_text_y);
        menuSprite.drawString(title, 50, animated_text_y);

        ui_run_easing(&animated_text_y, target_y, step);
        
        // Push the completed sprite to the screen in one go
        menuSprite.pushSprite(0, 0);

        vTaskDelay(pdMS_TO_TICKS(15));
    }

    current_state = entering ? SUB_MENU : MAIN_MENU;
}

// Main menu navigation
void showMenu() {
    if (current_state != MAIN_MENU) return;
    
    int direction = readEncoder();
    if (direction != 0) {
        // Play tick sound on scroll
        tone(BUZZER_PIN, 1000*(picture_flag + 1), 20);

        current_state = ANIMATING;
        
        if (direction == 1) { // Right
            picture_flag = (picture_flag + 1) % MENU_ITEM_COUNT;
        } else if (direction == -1) { // Left
            picture_flag = (picture_flag == 0) ? MENU_ITEM_COUNT - 1 : picture_flag - 1;
        }
        
        int16_t target_display = INITIAL_X_OFFSET - (picture_flag * ICON_SPACING);
        
        for (uint8_t step = ANIMATION_STEPS; step > 0; step--) {
            ui_run_easing(&display, target_display, step);
            drawMenuIcons(display);
            vTaskDelay(pdMS_TO_TICKS(15));
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
            case 0: BuzzerMenu(); break;
            case 1: weatherMenu(); break;
            case 2: performanceMenu(); break;
            case 3: LEDMenu(); break;
            case 4: DS18B20Menu(); break;
            case 5: AnimationMenu();break;
        }
    }
}