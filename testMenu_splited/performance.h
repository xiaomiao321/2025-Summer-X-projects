#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "img.h"

// -----------------------------
// 宏定义
// -----------------------------
#define BG_COLOR       0x0000
#define TITLE_COLOR    0xFFFF
#define VALUE_COLOR    0xFFFF
#define ERROR_COLOR    0xF800
#define LINE_HEIGHT    12
#define DATA_X         50
#define DATA_Y         5
#define LOGO_X         5
#define LOGO_Y_TOP     20
#define LOGO_Y_BOTTOM  80
#define BUFFER_SIZE    256

// -----------------------------
// 函数声明
// -----------------------------
void performanceMenu();
void drawPerformanceStaticElements();
void updatePerformanceData();
void showPerformanceError(const char *msg);
void resetBuffer();
void parsePCData();
void Performance_Init_Task(void *pvParameters);
void Performance_Task(void *pvParameters);
void SERIAL_Task(void *pvParameters);

#endif
