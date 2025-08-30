#include "LED.h"
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "Menu.h" // For access to tft object
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Helper for double click detection
bool led_detectDoubleClick() {
    static unsigned long lastClickTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastClickTime > 100 && currentTime - lastClickTime < 400) {
        lastClickTime = 0; // Reset for the next double click
        return true;
    }
    lastClickTime = currentTime;
    return false;
}

// Helper to draw the menu UI
void drawLedMenu(const char* mode, int value, int max_value, uint16_t bar_color) {
    tft.fillRect(0, 0, 240, 80, TFT_BLACK); // Clear top part of the screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 20);
    tft.printf("LED: %s", mode);

    // Draw progress bar
    int barWidth = map(value, 0, max_value, 0, 200);
    tft.fillRect(20, 60, 200, 20, TFT_DARKGREY);
    tft.fillRect(20, 60, barWidth, 20, bar_color);
}

void LEDMenu() {
    tft.fillScreen(TFT_BLACK);
    initRotaryEncoder();

    enum ControlMode { BRIGHTNESS_MODE, COLOR_MODE };
    ControlMode currentMode = BRIGHTNESS_MODE;

    uint8_t brightness = strip.getBrightness();
    if (brightness == 0) { // If brightness was 0, it's not visible. Set to a default.
        brightness = 50;
    }
    uint16_t hue = 0; // Start with red

    // Initial display
    drawLedMenu("Brightness", brightness, 255, TFT_YELLOW);
    strip.setBrightness(brightness);
    strip.fill(strip.ColorHSV(hue));
    strip.show();

    while (true) {
        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            if (currentMode == BRIGHTNESS_MODE) {
                int newBrightness = brightness + (encoderChange * 5); // Change by 5 for faster control
                if (newBrightness < 0) newBrightness = 0;
                if (newBrightness > 255) newBrightness = 255;
                brightness = newBrightness;
                strip.setBrightness(brightness);
                strip.show(); 
                drawLedMenu("Brightness", brightness, 255, TFT_YELLOW);
            } else { // COLOR_MODE
                int newHue = hue + (encoderChange * 1024); // Coarser change for hue
                if (newHue < 0) newHue = 65535 + newHue;
                hue = newHue % 65536;
                uint32_t newColor = strip.ColorHSV(hue);
                strip.fill(newColor);
                strip.show();
                
                uint16_t r = (newColor >> 16) & 0xFF;
                uint16_t g = (newColor >> 8) & 0xFF;
                uint16_t b = newColor & 0xFF;
                uint16_t barColor = tft.color565(r,g,b);
                drawLedMenu("Color", hue, 65535, barColor);
            }
        }

        if (readButton()) {
            if (led_detectDoubleClick()) {
                // Double click to exit
                break;
            } else {
                // Single click to toggle mode
                if (currentMode == BRIGHTNESS_MODE) {
                    currentMode = COLOR_MODE;
                    uint32_t currentColor = strip.getPixelColor(0);
                    uint16_t r = (currentColor >> 16) & 0xFF;
                    uint16_t g = (currentColor >> 8) & 0xFF;
                    uint16_t b = currentColor & 0xFF;
                    uint16_t barColor = tft.color565(r,g,b);
                    drawLedMenu("Color", hue, 65535, barColor);
                } else {
                    currentMode = BRIGHTNESS_MODE;
                    drawLedMenu("Brightness", brightness, 255, TFT_YELLOW);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}