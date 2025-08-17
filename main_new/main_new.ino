#include <Arduino.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "menu.h"
#include "RotaryEncoder.h"
#include "Weather.h"
#include "Performance.h"

// -----------------------------
// 全局变量定义
// -----------------------------
TFT_eSPI tft = TFT_eSPI();
int16_t display = 48;
int16_t display_trg = 1;
uint8_t picture_flag = 0;
uint8_t circle_num = 0;
const char *words[] = {"GAME", "WEATHER", "PERFORMANCE", "MESSAGE", "SETTING"};
const int speed_choose = 4;
const int icon_size = 48;

// -----------------------------
// 初始化 TFT 显示屏和菜单
// -----------------------------
void initMenu() {
  tft.init();
  tft.setRotation(1); // 横屏 128x128
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  showMenuConfig();
}
void Menu_Init_Task(void *pvParameters) {
  initMenu();
  initRotaryEncoder(); // 初始化旋转编码器
  vTaskDelete(NULL);
}

// -----------------------------
// 主菜单任务
// -----------------------------
void Menu_Task(void *pvParameters) {
  for (;;) {
    showMenu();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// -----------------------------
// setup() & loop()
// -----------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  xTaskCreatePinnedToCore(Menu_Init_Task, "Menu_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(Menu_Task, "Menu_Show", 8192, NULL, 1, NULL, 0);
}

void loop() {
  delay(1000);
}