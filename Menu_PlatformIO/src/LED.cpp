#include "LED.h"
#include <Adafruit_NeoPixel.h> 

bool stopLEDTask = false;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);


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
    tft.setCursor(0, 50);
    if (ledState) {
        tft.println("LED: ON");
    } else {
        tft.println("LED: OFF");
    }
}
void LED_Init_Task(void *pvParameters) {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();
  Serial.println("WS2812 LED 初始化完成");
  vTaskDelete(NULL);
}

void LED_Task(void *pvParameters) {
  for (;;) {
    // 检查是否需要退出
    if (stopLEDTask) {
      strip.clear();
      strip.show(); // 关闭所有灯
      Serial.println("LED_Task 安全退出");
      break;
    }

    colorWipe(strip.Color(255, 0, 0), 50);
    if (stopLEDTask) break;

    colorWipe(strip.Color(0, 255, 0), 50);
    if (stopLEDTask) break;

    colorWipe(strip.Color(0, 0, 255), 50);
    if (stopLEDTask) break;

    rainbow(20);
    if (stopLEDTask) break;

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  vTaskDelete(NULL); // 自我删除
}

void LEDMenu() {
    stopLEDTask = false;
    bool ledState = false; // 默认关闭

    animateMenuTransition("LED", true);
    xTaskCreatePinnedToCore(LED_Init_Task, "LED_Init", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(LED_Task, "LED_Show", 4096, NULL, 1, NULL, 0);

    initRotaryEncoder(); // 初始化旋转编码器
    updateScreen(ledState); // 初始屏幕更新

    while (true) {
        int encoderChange = readEncoder();
        if (encoderChange != 0) { // 检测到旋转
            ledState = !ledState; // 切换LED状态
            updateScreen(ledState); // 更新屏幕显示
            
            // 根据新状态调整LED
            if (ledState) {
                colorWipe(strip.Color(255, 0, 0), 50); // 打开LED
            } else {
                strip.clear();
                strip.show(); // 关闭所有LED
            }
        }

        if (readButton()) { // 如果检测到按钮按下，则退出
            Serial.println("Exiting LED Menu...");
            stopLEDTask = true;
            
            // 等待任务自我删除（最多等待1.5秒）
            TaskHandle_t task = xTaskGetHandle("LED_Show");
            for (int i = 0; i < 150 && task != NULL; i++) {
                if (eTaskGetState(task) == eDeleted) break;
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            
            TaskHandle_t initTask = xTaskGetHandle("LED_Init");
            if(initTask != NULL) {
                vTaskDelete(initTask);
            }
            
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    animateMenuTransition("LED", false);
    display = 48;
    picture_flag = 0;
    showMenuConfig();
}