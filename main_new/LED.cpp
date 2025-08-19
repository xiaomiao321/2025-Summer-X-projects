#include "led.h"

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void LED_Init_Task(void *pvParameters) {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();
  Serial.println("WS2812 LED 初始化完成");
  vTaskDelete(NULL);
}

void LED_Task(void *pvParameters) {
  for (;;) {
    colorWipe(strip.Color(255, 0, 0), 50); // 红色
    colorWipe(strip.Color(0, 255, 0), 50); // 绿色
    colorWipe(strip.Color(0, 0, 255), 50); // 蓝色
    rainbow(20); // 彩虹渐变
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
  Serial.print("颜色填充完成: ");
  Serial.println(color, HEX);
}

void rainbow(int wait) {
  for (long firstPixelHue = 0; firstPixelHue < 3 * 65536; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
  Serial.println("彩虹渐变完成");
}