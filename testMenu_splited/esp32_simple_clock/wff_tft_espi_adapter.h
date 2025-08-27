#ifndef WFF_TFT_ESPI_ADAPTER_H
#define WFF_TFT_ESPI_ADAPTER_H

#include <TFT_eSPI.h>
#include "typedefs.h"
#include "util.h"

// --- 全局变量 ---
// 让适配层和表盘逻辑都能访问到时间
extern timeDate_s* timeDate;

// --- 适配器函数 ---
void adapter_init(TFT_eSPI* tft_instance);

// --- 实现 watchface_api.h 中的函数 ---
millis_t millis_get();
byte div10(byte val);
byte mod10(byte val);
void watchFace_switchUpdate(void);
void setVectorDrawClipWin(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void drawVectorChar(int16_t x, int16_t y, unsigned char c,
                    uint16_t color, uint16_t bg, uint8_t size_x,
                    uint8_t size_y);

#endif // WFF_TFT_ESPI_ADAPTER_H
