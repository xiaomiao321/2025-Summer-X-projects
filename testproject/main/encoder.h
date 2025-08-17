#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

#define ENCODER_CLK 1
#define ENCODER_DT 0
#define ENCODER_SW 7
#define SWAP_CLK_DT 1

extern volatile int encoderPos;
extern int activeTab;

void initEncoder();
void Encoder_Task(void *pvParameters);

#endif