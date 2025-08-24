#include "Performance.h"
#include <TFT_eWidget.h>
#include "Menu.h"
#include "RotaryEncoder.h"
// -----------------------------
// 🔧 配置区
// -----------------------------

// -----------------------------
// 全局变量
// -----------------------------
struct PCData {
  char cpuName[64];
  char gpuName[64];
  int cpuTemp;
  int cpuLoad;
  int gpuTemp;
  int gpuLoad;
  float ramLoad;
  bool valid;
};
float esp32c3_temp = 0.0f;
struct PCData pcData = {.cpuName = "Unknown",
                        .gpuName = "Unknown",
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

GraphWidget cpuGpuLoadChart = GraphWidget(&tft);
TraceWidget cpuLoadTrace = TraceWidget(&cpuGpuLoadChart);
TraceWidget gpuLoadTrace = TraceWidget(&cpuGpuLoadChart);

GraphWidget cpuGpuTempChart = GraphWidget(&tft);
TraceWidget cpuTempTrace = TraceWidget(&cpuGpuTempChart);
TraceWidget gpuTempTrace = TraceWidget(&cpuGpuTempChart);

// 绘制静态元素
void drawPerformanceStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.pushImage(LOGO_X, LOGO_Y_TOP, 40, 40, NVIDIAGEFORCE);
  tft.pushImage(LOGO_X, LOGO_Y_BOTTOM - 16, 40, 40, intelcore); 
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Performance", DATA_X, DATA_Y);
  tft.drawString("CPU:", DATA_X, DATA_Y + LINE_HEIGHT);
  tft.drawString("GPU:", DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.drawString("RAM:", DATA_X, DATA_Y + 3 * LINE_HEIGHT);
  tft.drawString("ESP Temp:", DATA_X, DATA_Y + 4 * LINE_HEIGHT);

  cpuGpuLoadChart.createGraph(100, 40, tft.color565(5, 5, 5));
  cpuGpuLoadChart.setGraphScale(0.0, 100.0, 0.0, 100.0);
  cpuGpuLoadChart.setGraphGrid(0.0, 50.0, 0.0, 50.0, TFT_DARKGREY);
  cpuGpuLoadChart.drawGraph(80, 80);
  tft.fillCircle(200, 90, 5, TFT_GREEN);
  tft.fillCircle(200, 100, 5, TFT_BLUE);
  tft.drawString("CPU", 190, 90);
  tft.drawString("GPU", 190, 100);
  cpuLoadTrace.startTrace(TFT_GREEN);
  gpuLoadTrace.startTrace(TFT_BLUE);

  cpuGpuTempChart.createGraph(100, 40, tft.color565(5, 5, 5));
  cpuGpuTempChart.setGraphScale(0.0, 100.0, 0.0, 100.0);
  cpuGpuTempChart.setGraphGrid(0.0, 50.0, 0.0, 50.0, TFT_DARKGREY);
  cpuGpuTempChart.drawGraph(80, 130);
  tft.fillCircle(200, 140, 5, TFT_RED);
  tft.fillCircle(200, 150, 5, TFT_ORANGE);
  tft.drawString("RAM", 190, 140);
  tft.drawString("ESP32", 190, 150);
  cpuTempTrace.startTrace(TFT_RED);
  gpuTempTrace.startTrace(TFT_ORANGE);
}

// 更新 PC 数据
void updatePerformanceData() {
  if (xSemaphoreTake(xPCDataMutex, 10) != pdTRUE)
    return;
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  if (pcData.valid) {
    tft.fillRect(DATA_X + 40, DATA_Y + LINE_HEIGHT, 80, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.cpuLoad) + "%", DATA_X + 40, DATA_Y + LINE_HEIGHT);
    tft.fillRect(DATA_X + 40, DATA_Y + 2 * LINE_HEIGHT, 80, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.gpuLoad) + "%", DATA_X + 40, DATA_Y + 2 * LINE_HEIGHT);
    tft.fillRect(DATA_X + 40, DATA_Y + 3 * LINE_HEIGHT, 80, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.ramLoad, 1) + "%", DATA_X + 40, DATA_Y + 3 * LINE_HEIGHT);

    static float gx = 0.0;
    cpuLoadTrace.addPoint(gx, pcData.cpuLoad);
    gpuLoadTrace.addPoint(gx, pcData.gpuLoad);
    cpuTempTrace.addPoint(gx, pcData.cpuTemp);
    gpuTempTrace.addPoint(gx, pcData.gpuTemp);
    gx += 1.0;
    if (gx > 100.0) {
      gx = 0.0;
      cpuGpuLoadChart.drawGraph(14, 60);
      cpuLoadTrace.startTrace(TFT_GREEN);
      gpuLoadTrace.startTrace(TFT_BLUE);
      cpuGpuTempChart.drawGraph(14, 110);
      cpuTempTrace.startTrace(TFT_RED);
      gpuTempTrace.startTrace(TFT_ORANGE);
    }

  } else {
    tft.fillRect(DATA_X + 40, DATA_Y + LINE_HEIGHT, 80, 3 * LINE_HEIGHT, BG_COLOR);
    tft.setTextColor(ERROR_COLOR, BG_COLOR);
    tft.drawString("No Data", DATA_X + 40, DATA_Y + LINE_HEIGHT);
  }
  tft.fillRect(DATA_X + 60, DATA_Y + 4 * LINE_HEIGHT, 60, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(String(esp32c3_temp, 1) + "°C", DATA_X + 60, DATA_Y + 4 * LINE_HEIGHT);
  xSemaphoreGive(xPCDataMutex);
}

// 重置缓冲区
void resetBuffer() {
  bufferIndex = 0;
  inputBuffer[0] = '\0';
}

// 解析 PC 数据
void parsePCData() {
  struct PCData newData = {.cpuName = "Unknown",
                           .gpuName = "Unknown",
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
// 性能监控初始化任务
// -----------------------------
void Performance_Init_Task(void *pvParameters) {
  drawPerformanceStaticElements();
  vTaskDelete(NULL);
}

// -----------------------------
// 性能监控显示任务
// -----------------------------
void Performance_Task(void *pvParameters) {
  for (;;)
 {
    esp32c3_temp = temperatureRead();
    updatePerformanceData();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Update every second for smoother chart updates
  }
}

// 串口接收任务
void SERIAL_Task(void *pvParameters) {
  for (;;)
 {
    if (Serial.available()) {
      char inChar = (char)Serial.read();
      if (bufferIndex < BUFFER_SIZE - 1) {
        inputBuffer[bufferIndex++] = inChar;
        inputBuffer[bufferIndex] = '\0';
      } else {
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
  animateMenuTransition("PERFORMANCE",true);
  xPCDataMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(Performance_Init_Task, "Perf_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(Performance_Task, "Perf_Show", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(SERIAL_Task, "Serial_Rx", 2048, NULL, 1, NULL, 0);
  while (1) {
    if (readButton()) {
      animateMenuTransition("PERFORMANCE",false);
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