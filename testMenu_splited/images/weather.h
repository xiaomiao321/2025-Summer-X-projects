// weather.h 修改内容
#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "Menu.h"
#include "config.h"

extern TaskHandle_t weatherTaskHandle; // Declare weatherTaskHandle as extern

// 时间调整模式
enum TimeAdjustMode {
  MODE_NORMAL,
  MODE_ADJUST_HOUR,
  MODE_ADJUST_MINUTE
};

void weatherMenu();
bool connectWiFi();
void disconnectWiFi();
void showWiFiStatus(bool connected);
void Weather_Init_Task(void *pvParameters);
void Weather_Task(void *pvParameters);

#endif