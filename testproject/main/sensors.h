#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include "config.h"

extern Adafruit_AHTX0 aht;
extern float aht_temp, aht_hum;
extern String weather_main, weather_temp, weather_hum;
extern bool aht_ok;
extern unsigned long last_weather_update;

void AHT_Init_Task(void *pvParameters);
void AHT_Task(void *pvParameters);

#endif