#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define BUZZER_PIN 5

void Buzzer_Init_Task(void *pvParameters);
void Buzzer_Task(void *pvParameters);

#endif