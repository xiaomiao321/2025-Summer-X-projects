#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// TFT 布局（128x128 屏幕）
#define LOGO_X 10
#define LOGO_Y_TOP 24
#define LOGO_Y_BOTTOM 64
#define DATA_X 3
#define DATA_Y 24
#define LINE_HEIGHT 16
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

#define BUFFER_SIZE 512

// PC 数据结构体
struct PCData {
  char cpuName[64];
  char gpuName[64];
  char rpm[16];
  int cpuTemp;
  int cpuLoad;
  int gpuTemp;
  int gpuLoad;
  float ramLoad;
  bool valid;
};

extern Adafruit_AHTX0 aht;
extern float aht_temp;
extern float aht_hum;
extern bool aht_ok;
extern float esp32c3_temp;
extern struct PCData pcData;
extern char inputBuffer[BUFFER_SIZE];
extern uint16_t bufferIndex;
extern bool stringComplete;
extern SemaphoreHandle_t xPCDataMutex;

void performanceMenu();

#endif