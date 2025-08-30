#include <TFT_eSPI.h>
#include "Settings.h"
#include "RotaryEncoder.h"
#include "Buzzer.h"

// Assume tft is a global or accessible object
extern TFT_eSPI tft;
extern bool buzzerEnabled;

static int selectedItem = 0;
const char* menuItems[] = {"Buzzer", "Back"};
const int itemCount = sizeof(menuItems) / sizeof(menuItems[0]);

void drawSettingsMenu() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Settings", tft.width() / 2, 20);

    for (int i = 0; i < itemCount; i++) {
        tft.setTextDatum(MC_DATUM);
        if (i == selectedItem) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.drawString(">", 20, 80 + i * 40);
        } else {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
        }

        if (i == 0) { // Buzzer item
            char buzzerStatus[15];
            sprintf(buzzerStatus, "Buzzer: %s", buzzerEnabled ? "On" : "Off");
            tft.drawString(buzzerStatus, tft.width() / 2, 80 + i * 40);
        } else {
            tft.drawString(menuItems[i], tft.width() / 2, 80 + i * 40);
        }
    }
}

void SettingsMenu() {
    selectedItem = 0;
    drawSettingsMenu();

    while (true) {
        int direction = readEncoder();
        if (direction != 0) {
            selectedItem = (selectedItem + direction + itemCount) % itemCount;
            if (buzzerEnabled) {
                tone(BUZZER_PIN, 1200, 20);
            }
            drawSettingsMenu();
        }

        if (readButton()) {
            if (buzzerEnabled) {
                tone(BUZZER_PIN, 1800, 50);
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Debounce

            switch (selectedItem) {
                case 0: // Buzzer
                    buzzerEnabled = !buzzerEnabled;
                    drawSettingsMenu(); // Redraw to show new status
                    break;
                case 1: // Back
                    return; // Exit the settings menu
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}