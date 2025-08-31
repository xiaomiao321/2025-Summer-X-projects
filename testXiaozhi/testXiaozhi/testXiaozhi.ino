#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Sys.h"
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite menuSprite = TFT_eSprite(&tft);
char wifiStatusStr[30] = "WiFi: Disconnected";
void setup() {
    pinMode(BUZZER_PIN, OUTPUT);
    Serial.begin(115200);
    tone(5,2000,500);
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    menuSprite.createSprite(239, 239);
    connectWiFi();
}

void loop() {

}

