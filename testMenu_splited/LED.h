#ifndef LED_H
#define LED_H


#define LED_PIN 3
#define NUM_LEDS 10
#define BRIGHTNESS 50


void LEDMenu();
void LED_Init_Task(void *pvParameters);
void LED_Task(void *pvParameters);

#endif