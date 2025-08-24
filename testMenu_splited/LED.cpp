#include "LED.h"
#include <Adafruit_NeoPixel.h> 
#include "Menu.h"
bool stopLEDTask = false;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
TaskHandle_t ledTaskHandle = NULL; // 用于跟踪 LED_Task

void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    if (stopLEDTask) return; // 提前退出
    strip.setPixelColor(i, color);
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
}

void rainbow(int wait) {
  for (long firstPixelHue = 0; firstPixelHue < 3 * 65536 && !stopLEDTask; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) {
      if (stopLEDTask) return;
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
}

// 更新屏幕显示函数
void updateScreen(bool ledState) {
    tft.fillScreen(TFT_BLACK); // 清除屏幕
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 50); // 轻微调整位置以优化显示
    tft.println(ledState ? "LED: ON" : "LED: OFF");
}

void LED_Init_Task(void *pvParameters) {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear(); // 确保初始化时关闭所有 LED
  strip.show();
  Serial.println("WS2812 LED 初始化完成");
  vTaskDelete(NULL);
}

void LED_Task(void *pvParameters) {
  bool *ledState = (bool *)pvParameters; // 接收 LED 状态指针
  
  while(!stopLEDTask) {
    if (!(*ledState)) { // 如果 LED 关闭
      strip.clear();
      strip.show();
      vTaskDelay(pdMS_TO_TICKS(100)); // 降低 CPU 占用
      continue;
    }

    // 仅在 ledState 为 true 时运行动画
    colorWipe(strip.Color(255, 0, 0), 50);
    if (stopLEDTask) break;

    colorWipe(strip.Color(0, 255, 0), 50);
    if (stopLEDTask) break;

    colorWipe(strip.Color(0, 0, 255), 50);
    if (stopLEDTask) break;

    rainbow(20);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // 任务退出前的清理工作
  strip.clear();
  strip.show();
  ledTaskHandle = NULL; // 清除任务句柄
  vTaskDelete(NULL); // 删除任务自身
}

void LEDMenu() {
    stopLEDTask = false;
    bool ledState = false; // 默认关闭

    animateMenuTransition("LED", true);
    xTaskCreatePinnedToCore(LED_Init_Task, "LED_Init", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(LED_Task, "LED_Show", 4096, &ledState, 1, &ledTaskHandle, 0);

    initRotaryEncoder(); // 初始化旋转编码器
    updateScreen(ledState); // 初始屏幕更新

    while (true) {
        int encoderChange = readEncoder();
        if (encoderChange != 0) { // 检测到旋转
            ledState = !ledState; // 切换 LED 状态
            updateScreen(ledState); // 更新屏幕显示
            Serial.printf("LED State: %s\n", ledState ? "ON" : "OFF");
        }

        if (readButton()) { // 检测按钮按下，退出
            Serial.println("Exiting LED Menu...");
            stopLEDTask = true;
            
            // 等待 LED_Task 退出
            if (ledTaskHandle != NULL) {
                for (int i = 0; i < 150; i++) {
                    if (eTaskGetState(ledTaskHandle) == eDeleted) break;
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }

            // 清理 LED
            strip.clear();
            strip.show();
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    animateMenuTransition("LED", false);
    display = 48;
    picture_flag = 0;
    showMenuConfig();
}