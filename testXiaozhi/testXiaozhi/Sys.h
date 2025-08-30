#ifndef SYS_H
#define SYS_H
#define BUZZER_PIN 5
#define BG_COLOR TFT_BLACK
#include <TFT_eSPI.h>
extern TFT_eSPI tft;
extern TFT_eSprite menuSprite;
const int LOG_MARGIN = 5;
const int LOG_LINE_HEIGHT = 12;
const int LOG_MAX_LINES = 16;
void tftLog(String text, uint16_t color);
bool connectWiFi(); 
bool ensureWiFiConnected();
#endif // !SYS_H