#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "Menu.h"

// TFT 布局
#define DATA_X 3
#define DATA_Y 24
#define LINE_HEIGHT 16
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

extern bool wifi_connected;
extern struct tm timeinfo; // Global timeinfo for watchfaces
extern char temperature[10];
extern char humidity[10];
extern char reporttime[25];
extern char lastSyncTimeStr[45]; // To display last sync time
extern char lastWeatherSyncStr[45]; // To display last weather sync status
extern bool synced;

void weatherMenu();
void syncTime();
bool connectWiFi();
bool fetchWeather();

// Silent versions for background updates
void silentSyncTime();
void silentFetchWeather();
bool ensureWiFiConnected();
void TimeUpdate_Task(void *pvParameters);

#endif