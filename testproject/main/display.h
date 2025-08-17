#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include "config.h"
#include "sensors.h"
#include "pcdata.h"
#include "network.h"
#define BAR_LENGTH 160
#define BAR_HEIGHT 10
#define BAR_MARGIN 8
#define BAR_X ((240 - BAR_LENGTH) / 2)
#define BAR_DAY_Y (DATA_Y + 5 * LINE_HEIGHT)
#define BAR_HOUR_Y (BAR_DAY_Y + BAR_HEIGHT + BAR_MARGIN)
#define BAR_MINUTE_Y (BAR_HOUR_Y + BAR_HEIGHT + BAR_MARGIN)
#define DAY_SECOND 86400

extern TFT_eSPI tft;
extern struct tm timeinfo;
extern volatile int encoderPos;
extern int activeTab;

void initTFT();
void drawStaticElements();
void drawMenu();
void drawTimeTab();
void drawPerformanceTab();
void drawSingleBar(int y, uint32_t current, uint32_t total);
void showError(const char* msg);
void TFT_Init_Task(void *pvParameters);
void TFT_Task(void *pvParameters);

#endif