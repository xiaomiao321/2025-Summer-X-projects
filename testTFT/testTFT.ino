#include <TFT_eSPI.h>       

TFT_eSPI tft = TFT_eSPI();  

void setup() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawString("Hello ESP32-C3 Super Mini!", 10, 10, 2);
}

void loop() {
}