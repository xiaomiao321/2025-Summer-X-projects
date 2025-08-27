#include "wff_tft_espi_adapter.h"

static TFT_eSPI* _tft; // 指向主文件中的tft对象的本地指针

void adapter_init(TFT_eSPI* tft_instance) {
  _tft = tft_instance;
}

millis_t millis_get() {
  return millis();
}

byte div10(byte val) {
  return val / 10;
}

byte mod10(byte val) {
  return val % 10;
}

// 在我们的实现中，这个函数可以什么都不做
void watchFace_switchUpdate(void) {
  // no-op
}

// 用TFT_eSPI的setViewport实现
void setVectorDrawClipWin(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    // 注意：主程序已经设置过一次viewport了
    // 这里的坐标是相对于那个viewport的，所以我们直接使用
    _tft->setViewport(x0, y0, x1 - x0, y1 - y0);
}

// 关键的替换函数
void drawVectorChar(int16_t x, int16_t y, unsigned char c,
                    uint16_t color, uint16_t bg, uint8_t size_x,
                    uint8_t size_y) {
    
    // 1. 转换颜色 (原始代码 1=FG, 0=BG)
    uint16_t fg_color = (color == 1) ? TFT_WHITE : TFT_BLACK;
    // 背景色我们直接用透明背景，所以用fg_color
    uint16_t bg_color = TFT_TRANSPARENT;

    // 2. 估算字体大小
    int text_size = size_y / 7;
    if (text_size < 1) text_size = 1;

    // 3. 使用TFT_eSPI绘制字符
    _tft->setTextColor(fg_color); // 设置前景色
    _tft->setTextSize(text_size);
    _tft->setCursor(x, y);
    _tft->print((char)c);
}
