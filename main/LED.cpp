#include "LED.h"
#include "Alarm.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include <TFT_eSPI.h>
#include "Menu.h"
#include "MQTT.h" // For access to menuSprite
#include <Adafruit_NeoPixel.h>

// Initialize the NeoPixel strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Enum for control modes
enum ControlMode { BRIGHTNESS_MODE, COLOR_MODE };

// Helper function to detect a double click
bool led_detectDoubleClick() {
    static unsigned long lastClickTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastClickTime > 50 && currentTime - lastClickTime < 400) {
        lastClickTime = 0; // Reset for the next double click
        return true;
    }
    lastClickTime = currentTime;
    return false;
}

// Redesigned UI drawing function using menuSprite
void drawLedControl(uint8_t brightness, uint16_t hue, ControlMode currentMode) {
    menuSprite.fillSprite(TFT_BLACK); // Clear sprite to prevent artifacts
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2);
    menuSprite.setTextDatum(TL_DATUM);

    // --- Brightness Control ---
    menuSprite.drawString("Brightness", 20, 20);
    if (currentMode == BRIGHTNESS_MODE) {
        menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
        menuSprite.drawString("<", 200, 20); // Active indicator
    }
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    int brightnessBarWidth = map(brightness, 0, 255, 0, 200);
    menuSprite.fillRect(20, 50, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 50, brightnessBarWidth, 20, TFT_YELLOW);

    // --- Color Control ---
    menuSprite.drawString("Color", 20, 90);
    if (currentMode == COLOR_MODE) {
        menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
        menuSprite.drawString("<", 200, 90); // Active indicator
    }
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    int hueBarWidth = map(hue, 0, 65535, 0, 200);
    uint32_t color = strip.ColorHSV(hue);
    uint16_t r = (color >> 16) & 0xFF;
    uint16_t g = (color >> 8) & 0xFF;
    uint16_t b = color & 0xFF;
    uint16_t barColor = tft.color565(r, g, b);
    menuSprite.fillRect(20, 120, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 120, hueBarWidth, 20, barColor);

    // --- Instructions ---
    menuSprite.setTextDatum(BC_DATUM);
    menuSprite.setTextSize(1);
    menuSprite.drawString("Click: Toggle | Dbl-Click: Exit", 120, 230);
}

void LEDMenu() {
    initRotaryEncoder();

    ControlMode currentMode = BRIGHTNESS_MODE;
    uint8_t brightness = strip.getBrightness();
    if (brightness == 0) { // If LEDs were off, start at a visible brightness
        brightness = 128;
    }
    uint16_t hue = 0; // Start with red

    // Initial setup
    strip.setBrightness(brightness);
    strip.fill(strip.ColorHSV(hue));
    strip.show();

    // Initial draw
    drawLedControl(brightness, hue, currentMode);
    menuSprite.pushSprite(0, 0);

    bool needsRedraw = true;

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            break;
        }
        if (g_alarm_is_ringing) { break; } // ADDED LINE
        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            needsRedraw = true;
            if (currentMode == BRIGHTNESS_MODE) {
                // Increased sensitivity for brightness
                int newBrightness = brightness + (encoderChange * 10); 
                if (newBrightness < 0) newBrightness = 0;
                if (newBrightness > 255) newBrightness = 255;
                brightness = newBrightness;
                strip.setBrightness(brightness);
            } else { // COLOR_MODE
                // Increased sensitivity for color
                int newHue = hue + (encoderChange * 2048); 
                if (newHue < 0) newHue = 65535 + newHue;
                hue = newHue % 65536;
            }
        }

        if (readButton()) {
            needsRedraw = true;
            if (led_detectDoubleClick()) {
                break; // Exit on double click
            } else {
                // Toggle mode on single click
                currentMode = (currentMode == BRIGHTNESS_MODE) ? COLOR_MODE : BRIGHTNESS_MODE;
            }
        }

        if (needsRedraw) {
            strip.fill(strip.ColorHSV(hue));
            strip.show();
            drawLedControl(brightness, hue, currentMode);
            menuSprite.pushSprite(0, 0); // Push the complete frame to the screen
            needsRedraw = false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
