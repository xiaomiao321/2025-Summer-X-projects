#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define LED_PIN 3
#define NUM_LEDS 10
#define BRIGHTNESS 50

extern Adafruit_NeoPixel strip;

void LED_Init_Task(void *pvParameters);
void LED_Task(void *pvParameters);

#endif