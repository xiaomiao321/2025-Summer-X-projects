#ifndef PCDATA_H
#define PCDATA_H

#include "config.h"
#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
struct PCData {
  char cpuName[64] = "Unknown";
  char gpuName[64] = "Unknown";
  char rpm[16] = "Unknown";
  int cpuTemp = 0;
  int cpuLoad = 0;
  int gpuTemp = 0;
  int gpuLoad = 0;
  float ramLoad = 0.0;
  bool valid = false;
};

extern struct PCData pcData;
extern SemaphoreHandle_t xPCDataMutex;
extern char inputBuffer[BUFFER_SIZE];
extern uint16_t bufferIndex;
extern bool stringComplete;
extern float esp32c3_temp;

void resetBuffer();
void parsePCData();
void SERIAL_Task(void *pvParameters);

#endif