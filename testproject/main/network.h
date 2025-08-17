#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
extern bool wifi_connected;
extern unsigned long last_time_update;
extern String weather_main;
extern String weather_temp;
extern String weather_hum;
extern struct tm timeinfo;

bool connectWiFi();
void disconnectWiFi();
bool syncNTPTime();
bool getWeather();
void WIFI_Init_Task(void *pvParameters);

#endif