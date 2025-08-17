#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>

// TFT 布局（128x128 屏幕）
#define DATA_X 3
#define DATA_Y 24
#define LINE_HEIGHT 16
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

extern struct tm timeinfo;
extern String weather_main;
extern String weather_temp;
extern String weather_hum;
extern bool wifi_connected;
extern unsigned long last_weather_update;
extern unsigned long last_time_update;
extern int last_sec;

void weatherMenu();

#endif