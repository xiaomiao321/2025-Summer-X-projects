#include <TFT_eSPI.h>
#include "wff_tft_espi_adapter.h"
#include "wff_simple_clock.h"

TFT_eSPI tft = TFT_eSPI();

// --- 全局变量 ---
// 适配层需要访问的全局时间变量
timeDate_s global_time_date;
// 指向该变量的指针，供旧代码使用
timeDate_s* timeDate = &global_time_date;


void setup() {
  Serial.begin(115200);
  
  // 1. 初始化TFT屏幕
  tft.begin();
  tft.setRotation(0); // 根据你的屏幕方向设置
  tft.fillScreen(TFT_BLACK);

  // 2. 初始化时间和适配层
  // TODO: 在这里添加从NTP或RTC获取真实时间的代码
  // 下面是用于测试的假数据
  global_time_date.time.hour = 10;
  global_time_date.time.mins = 30;
  global_time_date.time.secs = 0;

  // 将tft实例传递给适配器
  adapter_init(&tft); 
  
  // 3. 在屏幕中央创建一个128x64的“虚拟画布”
  // 这样原始代码的坐标就可以直接使用
  int startX = (tft.width() - 128) / 2;
  int startY = (tft.height() - 64) / 2;
  tft.setViewport(startX, startY, 128, 64);

  // 4. 调用一次表盘的初始化函数 (如果有的话)
  simple_clock_init();
}

void loop() {
  // 1. 更新时间 (填充timeDate结构体)
  // TODO: 在这里添加从NTP或RTC获取真实时间的代码
  // 简单地让秒数递增用于测试
  global_time_date.time.secs++;
  if (global_time_date.time.secs >= 60) {
    global_time_date.time.secs = 0;
    global_time_date.time.mins++;
    if (global_time_date.time.mins >= 60) {
      global_time_date.time.mins = 0;
      global_time_date.time.hour++;
      if (global_time_date.time.hour >= 24) {
        global_time_date.time.hour = 0;
      }
    }
  }

  // 2. 清理虚拟画布
  tft.fillScreen(TFT_BLACK); // 这只会清空128x64的viewport

  // 3. 调用表盘的绘图函数
  simple_clock_draw();

  // 4. 延迟，控制刷新率
  delay(200); 
}
