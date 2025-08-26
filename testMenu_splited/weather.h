#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "Menu.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// TFT 布局
#define DATA_X 3
#define DATA_Y 24
#define LINE_HEIGHT 16
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

struct WeatherData {
  String province;
  String city;
  String weather;
  String temperature;
  String humidity;
  String reporttime;
  bool valid = false;
};

extern WeatherData weatherData;
extern bool wifi_connected;

void weatherMenu();

#endif
