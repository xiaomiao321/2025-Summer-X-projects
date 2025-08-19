#include <Adafruit_NeoPixel.h>  // 引入Adafruit_NeoPixel库，用于控制WS2812 LED

#define LED_PIN    3     // GPIO3作为数据引脚（DIN）
#define NUM_LEDS   10    // 假设级联10个LED，根据实际修改
#define BRIGHTNESS 50    // 亮度设置（0-255），中等亮度以符合规格书恒流特性

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);  // 初始化LED链，GRB顺序（规格书颜色：G/R/B）

void setup() {
  strip.begin();          // 初始化LED库
  strip.setBrightness(BRIGHTNESS);  // 设置亮度
  strip.show();           // 更新LED，初始全灭
  Serial.begin(115200);   // 串口初始化，用于调试输出
  Serial.println("WS2812C-X 测试程序启动");  // 输出中文提示
}

void loop() {
  // 测试1: 依次点亮红色、绿色、蓝色（测试基本颜色）
  colorWipe(strip.Color(255, 0, 0), 50);  // 红色填充
  colorWipe(strip.Color(0, 255, 0), 50);  // 绿色填充
  colorWipe(strip.Color(0, 0, 255), 50);  // 蓝色填充

  // 测试2: 彩虹渐变效果（测试级联和颜色混合）
  rainbow(20);  // 彩虹循环，延时20ms

  delay(1000);  // 每轮测试后暂停1秒
}

// 函数：颜色填充（从头到尾逐个点亮）
void colorWipe(uint32_t color, int wait) {
  for(int i = 0; i < strip.numPixels(); i++) {  // 遍历每个LED
    strip.setPixelColor(i, color);  // 设置颜色
    strip.show();                   // 更新显示
    delay(wait);                    // 延时
  }
  Serial.print("颜色填充完成: ");  // 调试输出
  Serial.println(color, HEX);      // 输出颜色值（十六进制）
}

// 函数：彩虹渐变
void rainbow(int wait) {
  for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {  //  hue从0到3*65536循环
    for(int i = 0; i < strip.numPixels(); i++) {  // 遍历每个LED
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());  // 计算hue
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));  // 设置HSV颜色
    }
    strip.show();  // 更新显示
    delay(wait);   // 延时
  }
  Serial.println("彩虹渐变完成");  // 调试输出
}