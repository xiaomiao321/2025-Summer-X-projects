#include "DS18B20.h"

#define DS18B20_PIN 10            // GPIO10作为DS18B20数据引脚
// 全局对象
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

bool stopDS18B20Task = false;

// 初始化任务
void DS18B20_Init_Task(void *pvParameters) {
  sensors.begin();

  DeviceAddress tempAddress;
  if (!sensors.getAddress(tempAddress, 0)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.println("❌ No Sensor Found");
    tft.println("Please check wiring or power");
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.println("✅ Sensor Ready");
    // 可选：打印 ROM 地址
    for (int i = 0; i < 8; i++) {
      tft.printf("%02X ", tempAddress[i]);
    }
  }

  delay(1000);
  vTaskDelete(NULL);
}

// 主任务：读取并显示温度
void DS18B20_Task(void *pvParameters) {
  char tempStr[32];
  float lastTemp = -274;
  unsigned long timeout;

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  tft.setCursor(30, 50);
  tft.println("Waiting...");

  while (1) {
    // ✅ 第一步：立即检查退出标志
    if (stopDS18B20Task) {
      tft.fillRect(0, 40, tft.width(), 60, TFT_BLACK);
      tft.setCursor(10, 50);
      tft.println("Stopped");
      delay(500);
      break;
    }

    // ✅ 第二步：发送请求
    sensors.requestTemperatures();

    // ✅ 第三步：等待转换完成（最多 750ms），但允许退出
    timeout = millis() + 750;
    while (millis() < timeout) {
      // ✅ 在等待中持续检查退出信号
      if (stopDS18B20Task) {
        goto exit_task;
      }
      vTaskDelay(pdMS_TO_TICKS(10)); // 每10ms检查一次
    }

    // ✅ 第四步：读取结果
    float tempC = sensors.getTempCByIndex(0);

    if (tempC != DEVICE_DISCONNECTED_C && tempC > -50 && tempC < 150) {
      if (tempC != lastTemp) {
        tft.fillRect(0, 40, tft.width(), 60, TFT_BLACK);
        sprintf(tempStr, "%.2f°C", tempC);
        int strWidth = tft.textWidth(tempStr);
        int xPos = (tft.width() - strWidth) / 2;
        tft.setCursor(xPos, 50);
        tft.setTextSize(4);
        tft.setTextColor(TFT_WHITE);
        tft.println(tempStr);
        lastTemp = tempC;
      }
    } else {
      if (stopDS18B20Task) break;

      tft.fillRect(0, 40, tft.width(), 60, TFT_BLACK);
      tft.setCursor(20, 50);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED);
      tft.println("Sensor Error");
    }

    // ✅ 每秒更新一次，但允许中途退出
    for (int i = 0; i < 100; i++) {
      if (stopDS18B20Task) goto exit_task;
      vTaskDelay(pdMS_TO_TICKS(10)); // 100×10ms = 1s
    }
  }

exit_task:
  vTaskDelete(NULL);
}

// 菜单入口
void DS18B20Menu() {
  stopDS18B20Task = false;

  animateMenuTransition("DS18B20", true);

  // 清屏并准备显示
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(3);
  tft.setCursor(20, 10);
  tft.println("DS18B20 Temperature");

  xTaskCreatePinnedToCore(DS18B20_Init_Task, "DS18B20_Init", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(DS18B20_Task,     "DS18B20_Task", 4096, NULL, 1, NULL, 0);

  while (1) {
  if (readButton()) {
    stopDS18B20Task = true; // 发送退出信号
    // 等待最多 1.5 秒让任务退出
    TaskHandle_t task = xTaskGetHandle("DS18B20_Task");
    for (int i = 0; i < 150 && task != NULL; i++) {
      if (eTaskGetState(task) == eDeleted) break;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    break;
  }
  vTaskDelay(pdMS_TO_TICKS(10)); 
}

  animateMenuTransition("DS18B20", false);
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}