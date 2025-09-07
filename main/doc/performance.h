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
#define LINE_HEIGHT    24
#define DATA_X         95
#define DATA_Y         10
#define LOGO_X         5
#define LOGO_Y_TOP     5
#define LOGO_Y_BOTTOM  75
#define BUFFER_SIZE    256

// Combined Chart Dimensions and Position
#define COMBINED_CHART_WIDTH    200
#define COMBINED_CHART_HEIGHT   80
#define COMBINED_CHART_X        20
#define COMBINED_CHART_Y        155

#define VALUE_OFFSET_X 40
#define VALUE_WIDTH    100
#define LEGEND_RADIUS  5
#define LEGEND_TEXT_OFFSET 8
#define LEGEND_X_START 95 // Shifted to the right
#define LEGEND_Y_POS 110
#define LEGEND_SPACING_WIDE 80
#define LEGEND_LINE_HEIGHT 15

// Arc display settings
#define ARC_CPU_X 40
#define ARC_CPU_Y 60
#define ARC_GPU_X 120
#define ARC_GPU_Y 60
#define ARC_RADIUS_MAX 35
#define ARC_RADIUS_MIN 15
#define ARC_THICKNESS 8
#define ARC_BG_COLOR TFT_DARKGREY
#define ARC_CPU_COLOR TFT_GREEN
#define ARC_GPU_COLOR TFT_BLUE

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
