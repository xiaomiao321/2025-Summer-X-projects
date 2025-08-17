#include <TFT_eSPI.h>

#include "img.h"

TFT_eSPI tft = TFT_eSPI(); 
void setup() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_RED);
  // 使用TFT_eSPI显示图片
  tft.pushImage(0, 0, NVIDIA_WIDTH, NVIDIA_HEIGHT, NVIDIA);
}



void loop() {
}