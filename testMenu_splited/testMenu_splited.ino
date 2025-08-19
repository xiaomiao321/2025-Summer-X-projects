#include <Arduino.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include "Menu.h"

// 全局变量
TFT_eSPI tft = TFT_eSPI();         // TFT显示屏对象
TFT_eSprite menuSprite = TFT_eSprite(&tft);
void setup() {
    initRotaryEncoder(); // 初始化旋转编码器
    tft.init(); // 初始化TFT显示屏
    tft.setRotation(1); // 设置屏幕旋转方向

    menuSprite.createSprite(128, 65);

    tft.fillScreen(TFT_BLACK); // 清屏
    showMenuConfig(); // 显示主菜单
}

void loop() {
    showMenu(); // 处理主菜单导航
    vTaskDelay(pdMS_TO_TICKS(15)); // 延时15ms
}