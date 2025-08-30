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
    if(millis() % 1000 < 500)
    {
    menuSprite.setTextDatum(BC_DATUM);
    menuSprite.setTextSize(1);
    if(ensureWiFiConnected())
    {
        menuSprite.setTextColor(TFT_GREEN, TFT_BLACK); // White for general status
    }
    else
    {
        menuSprite.setTextColor(TFT_RED,TFT_BLACK);
    }
    menuSprite.drawString(wifiStatusStr, 120, tft.height()-35); // 10 pixels above lastWeatherSyncStr
    menuSprite.setTextColor(TFT_WHITE,TFT_BLACK);
    }
}

