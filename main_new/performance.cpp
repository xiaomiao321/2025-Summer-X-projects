#include "Performance.h"
#include "Menu.h"
#include "img.h"
#include <Wire.h>
#include "RotaryEncoder.h"

// -----------------------------
// 🔧 配置区
// -----------------------------
const long SENSOR_INTERVAL_MS = 5000;

// -----------------------------
// 全局变量
// -----------------------------
Adafruit_AHTX0 aht;
float aht_temp = 0.0f, aht_hum = 0.0f;
bool aht_ok = false;
float esp32c3_temp = 0.0f;
struct PCData pcData = {.cpuName = "Unknown",
                        .gpuName = "Unknown",
                        .rpm = "Unknown",
                        .cpuTemp = 0,
                        .cpuLoad = 0,
                        .gpuTemp = 0,
                        .gpuLoad = 0,
                        .ramLoad = 0.0,
                        .valid = false};
char inputBuffer[BUFFER_SIZE];
uint16_t bufferIndex = 0;
bool stringComplete = false;
SemaphoreHandle_t xPCDataMutex = NULL;
extern TFT_eSPI tft; // 声明外部 TFT 对象

// -----------------------------
// 绘制静态元素
// -----------------------------
void drawPerformanceStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.pushImage(LOGO_X, LOGO_Y_TOP, 40, 40, NVIDIAGEFORCE);
  tft.pushImage(LOGO_X, LOGO_Y_BOTTOM, 40, 40, intelcore);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Performance", DATA_X, DATA_Y);
  tft.drawString("CPU Name:", DATA_X, DATA_Y + LINE_HEIGHT);
  tft.drawString("CPU Load:", DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.drawString("GPU Name:", DATA_X, DATA_Y + 3 * LINE_HEIGHT);
  tft.drawString("GPU Load:", DATA_X, DATA_Y + 4 * LINE_HEIGHT);
  tft.drawString("RAM Load:", DATA_X, DATA_Y + 5 * LINE_HEIGHT);
  tft.drawString("ESP32 Temp:", DATA_X, DATA_Y + 6 * LINE_HEIGHT);
}

// -----------------------------
// 更新传感器和 PC 数据
// -----------------------------
void updatePerformanceData() {
  if (xSemaphoreTake(xPCDataMutex, 10) != pdTRUE)
    return;
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  if (pcData.valid) {
    tft.fillRect(DATA_X + 60, DATA_Y + LINE_HEIGHT, 60, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.cpuName), DATA_X + 60, DATA_Y + LINE_HEIGHT);
    tft.fillRect(DATA_X + 60, DATA_Y + 2 * LINE_HEIGHT, 60, LINE_HEIGHT,
                 BG_COLOR);
    tft.drawString(String(pcData.cpuLoad) + "%", DATA_X + 60,
                   DATA_Y + 2 * LINE_HEIGHT);
    tft.fillRect(DATA_X + 60, DATA_Y + 3 * LINE_HEIGHT, 60, LINE_HEIGHT,
                 BG_COLOR);
    tft.drawString(String(pcData.gpuName), DATA_X + 60,
                   DATA_Y + 3 * LINE_HEIGHT);
    tft.fillRect(DATA_X + 60, DATA_Y + 4 * LINE_HEIGHT, 60, LINE_HEIGHT,
                 BG_COLOR);
    tft.drawString(String(pcData.gpuLoad) + "%", DATA_X + 60,
                   DATA_Y + 4 * LINE_HEIGHT);
    tft.fillRect(DATA_X + 60, DATA_Y + 5 * LINE_HEIGHT, 60, LINE_HEIGHT,
                 BG_COLOR);
    tft.drawString(String(pcData.ramLoad, 1) + "%", DATA_X + 60,
                   DATA_Y + 5 * LINE_HEIGHT);
  } else {
    tft.fillRect(DATA_X + 60, DATA_Y + LINE_HEIGHT, 60, 5 * LINE_HEIGHT,
                 BG_COLOR);
    tft.setTextColor(ERROR_COLOR, BG_COLOR);
    tft.drawString("No Data", DATA_X + 60, DATA_Y + LINE_HEIGHT);
  }
  tft.fillRect(DATA_X + 60, DATA_Y + 6 * LINE_HEIGHT, 60, LINE_HEIGHT,
               BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(String(esp32c3_temp, 1) + "°C", DATA_X + 60,
                 DATA_Y + 6 * LINE_HEIGHT);
  xSemaphoreGive(xPCDataMutex);
}

// -----------------------------
// 显示错误信息
// -----------------------------
void showPerformanceError(const char *msg) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(ERROR_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Error:", tft.width() / 2, tft.height() / 2 - LINE_HEIGHT);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  delay(2000);
  drawPerformanceStaticElements();
}

// -----------------------------
// 重置缓冲区
// -----------------------------
void resetBuffer() {
  bufferIndex = 0;
  inputBuffer[0] = '\0';
}

// -----------------------------
// 解析 PC 数据
// -----------------------------
void parsePCData() {
  struct PCData newData = {.cpuName = "Unknown",
                           .gpuName = "Unknown",
                           .rpm = "Unknown",
                           .cpuTemp = 0,
                           .cpuLoad = 0,
                           .gpuTemp = 0,
                           .gpuLoad = 0,
                           .ramLoad = 0.0,
                           .valid = false};
  char *ptr = nullptr;
  ptr = strstr(inputBuffer, "C");
  if (ptr) {
    char *degree = strchr(ptr, 'c');
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      newData.cpuTemp = atoi(temp);
      newData.cpuLoad = atoi(load);
    }
  }
  ptr = strstr(inputBuffer, "G");
  if (ptr) {
    char *degree = strstr(ptr, "c");
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      newData.gpuTemp = atoi(temp);
      newData.gpuLoad = atoi(load);
    }
  }
  ptr = strstr(inputBuffer, "RL");
  if (ptr) {
    ptr += 2;
    char *end = strchr(ptr, '|');
    if (end) {
      char load[8] = {0};
      strncpy(load, ptr, end - ptr);
      newData.ramLoad = atof(load);
    }
  }
  ptr = strstr(inputBuffer, "GRPM");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (end && end - ptr < sizeof(newData.rpm) - 1) {
      strncpy(newData.rpm, ptr, end - ptr);
      newData.rpm[end - ptr] = '\0';
    }
  }
  ptr = strstr(inputBuffer, "CPU:");
  if (ptr) {
    ptr += 4;
    char *end = strstr(ptr, "GPU:");
    if (!end)
      end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(newData.cpuName) - 1) {
      strncpy(newData.cpuName, ptr, end - ptr);
      newData.cpuName[end - ptr] = '\0';
      for (int i = strlen(newData.cpuName) - 1;
           i >= 0 && newData.cpuName[i] == ' '; i--)
        newData.cpuName[i] = '\0';
    }
  }
  ptr = strstr(inputBuffer, "GPU:");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (!end)
      end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(newData.gpuName) - 1) {
      strncpy(newData.gpuName, ptr, end - ptr);
      newData.gpuName[end - ptr] = '\0';
      for (int i = strlen(newData.gpuName) - 1;
           i >= 0 && newData.gpuName[i] == ' '; i--)
        newData.gpuName[i] = '\0';
    }
  }
  if (newData.cpuTemp > 0 || newData.gpuTemp > 0 || newData.ramLoad > 0) {
    newData.valid = true;
  }
  if (xSemaphoreTake(xPCDataMutex, portMAX_DELAY) == pdTRUE) {
    pcData = newData;
    xSemaphoreGive(xPCDataMutex);
  }
}

// -----------------------------
// AHT 传感器初始化任务
// -----------------------------
void AHT_Init_Task(void *pvParameters) {
  Wire.begin(20, 21);
  Serial.println("Initializing AHT...");
  aht_ok = aht.begin();
  if (aht_ok)
    Serial.println("✅ AHT sensor found!");
  else
    Serial.println("❌ AHT sensor not found!");
  vTaskDelete(NULL);
}

// -----------------------------
// AHT 传感器读取任务
// -----------------------------
void AHT_Task(void *pvParameters) {
  for (;;) {
    if (aht_ok) {
      sensors_event_t hum, temp;
      if (aht.getEvent(&hum, &temp)) {
        aht_temp = temp.temperature;
        aht_hum = hum.relative_humidity;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
  }
}

// -----------------------------
// 性能监控初始化任务
// -----------------------------
void Performance_Init_Task(void *pvParameters) {
  drawPerformanceStaticElements();
  xTaskCreatePinnedToCore(AHT_Init_Task, "AHT_Init", 4096, NULL, 2, NULL, 0);
  vTaskDelete(NULL);
}

// -----------------------------
// 性能监控显示任务
// -----------------------------
void Performance_Task(void *pvParameters) {
  for (;;) {
    esp32c3_temp = temperatureRead();
    updatePerformanceData();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// -----------------------------
// 串口接收任务
// -----------------------------
void SERIAL_Task(void *pvParameters) {
  for (;;) {
    if (Serial.available()) {
      char inChar = (char)Serial.read();
      if (bufferIndex < BUFFER_SIZE - 1) {
        inputBuffer[bufferIndex++] = inChar;
        inputBuffer[bufferIndex] = '\0';
      } else {
        showPerformanceError("Buffer full!");
        resetBuffer();
      }
      if (inChar == '\n') {
        stringComplete = true;
      }
    }
    if (stringComplete) {
      parsePCData();
      stringComplete = false;
      resetBuffer();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// -----------------------------
// 性能监控菜单入口
// -----------------------------
void performanceMenu() {
  toSubMenuDisplay("PERFORMANCE");
  xPCDataMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(Performance_Init_Task, "Perf_Init", 8192, NULL, 2,
                          NULL, 0);
  xTaskCreatePinnedToCore(AHT_Task, "AHT_Read", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(Performance_Task, "Perf_Show", 8192, NULL, 1, NULL,
                          0);
  xTaskCreatePinnedToCore(SERIAL_Task, "Serial_Rx", 2048, NULL, 1, NULL, 0);
  while (1) {
    if (readButton()) {
      subToMenuDisplay("PERFORMANCE");
      vTaskDelete(xTaskGetHandle("AHT_Read"));
      vTaskDelete(xTaskGetHandle("Perf_Show"));
      vTaskDelete(xTaskGetHandle("Serial_Rx"));
      vSemaphoreDelete(xPCDataMutex);
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}