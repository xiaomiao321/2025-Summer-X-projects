#include <Arduino.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>

// All menu headers
#include "Menu.h"
#include "LED.h"
#include "Buzzer.h"
#include "weather.h"
#include "performance.h"
#include "DS18B20.h"
#include "animation.h"
#include "Games.h"
#include "Countdown.h"
#include "Stopwatch.h"
#include "ADC.h"
#include "Watchface.h"

// Forward declaration for ADC setup function
void setupADC();

// 全局变量
TFT_eSPI tft = TFT_eSPI();         // TFT显示屏对象
TFT_eSprite menuSprite = TFT_eSprite(&tft);
void setup() {
    Buzzer_Init(); // 初始化蜂鸣器
    initRotaryEncoder(); // 初始化旋转编码器
    tft.init(); // 初始化TFT显示屏
    tft.setRotation(1); // 设置屏幕旋转方向

    setupADC(); // Initialize ADC

    menuSprite.createSprite(239, 239);

    tft.fillScreen(TFT_BLACK); // 清屏
    strip.begin();
    strip.show();
    showMenuConfig(); // 显示主菜单
}

void loop() {
    showMenu(); // 处理主菜单导航
    vTaskDelay(pdMS_TO_TICKS(15)); // 延时15ms
}