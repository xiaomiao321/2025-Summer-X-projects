#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include "Menu.h"
#include "DS18B20.h"
// TFT 布局（128x128 屏幕）
#define LOGO_X 10
#define LOGO_Y_TOP 24
#define LOGO_Y_BOTTOM 64
#define DATA_X 3
#define DATA_Y 24
#define LINE_HEIGHT 16
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

#define BUFFER_SIZE 512

// Chart layout
#define CHART_X 3
#define CHART_Y 80
#define CHART_WIDTH 122
#define CHART_HEIGHT 45
#define MAX_DATA_POINTS 10

// extern Adafruit_AHTX0 aht;
// extern float aht_temp;
// extern float aht_hum;
// extern bool aht_ok;
// extern float esp32c3_temp;
// extern struct PCData pcData;
// extern char inputBuffer[BUFFER_SIZE];
// extern uint16_t bufferIndex;
// extern bool stringComplete;
// extern SemaphoreHandle_t xPCDataMutex;
// extern struct PerformanceHistory perfHistory;

void performanceMenu();

#endif