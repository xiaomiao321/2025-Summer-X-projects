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
// 绘制静态元素
void drawPerformanceStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.pushImage(LOGO_X, LOGO_Y_TOP, NVIDIA_HEIGHT, NVIDIA_WIDTH, NVIDIA);
  tft.pushImage(LOGO_X, LOGO_Y_BOTTOM, INTEL_HEIGHT, INTEL_WIDTH, intel); 
  tft.setTextSize(2); 
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_GREEN, BG_COLOR);
  tft.drawString("CPU:", DATA_X, DATA_Y);
  tft.setTextColor(TFT_BLUE, BG_COLOR);
  tft.drawString("GPU:", DATA_X, DATA_Y + LINE_HEIGHT);
  tft.setTextColor(TFT_RED, BG_COLOR);
  tft.drawString("RAM:", DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.setTextColor(TFT_ORANGE, BG_COLOR);
  tft.drawString("ESP:", DATA_X, DATA_Y + 3 * LINE_HEIGHT);

  cpuGpuLoadChart.createGraph(CHART_WIDTH, CHART_HEIGHT, tft.color565(5, 5, 5));
  cpuGpuLoadChart.setGraphScale(0.0, 100.0, 0.0, 100.0);
  cpuGpuLoadChart.setGraphGrid(0.0, 25.0, 0.0, 25.0, TFT_DARKGREY);
  cpuGpuLoadChart.drawGraph(CHART1_X, CHART1_Y);
  
  // Legend for chart 1 (top right of chart)
  tft.setTextSize(1); // Smaller text for legend
  tft.fillCircle(DATA_X, LEGEND_ITEM_Y, LEGEND_RADIUS, TFT_GREEN);
  tft.setTextColor(TFT_GREEN, BG_COLOR);
  tft.drawString("CPU", DATA_X + LEGEND_TEXT_OFFSET, LEGEND_ITEM_Y);
  tft.fillCircle(DATA_X + LEGEND_SPACING, LEGEND_ITEM_Y, LEGEND_RADIUS, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, BG_COLOR);
  tft.drawString("GPU", DATA_X + LEGEND_SPACING + LEGEND_TEXT_OFFSET, LEGEND_ITEM_Y);

  // Axis values for chart 1
  tft.setTextColor(TITLE_COLOR, BG_COLOR); // Axis value color
  // tft.drawString("0", cpuGpuLoadChart.getPointX(0), CHART1_Y + CHART_HEIGHT + 5);
  // tft.drawString("50", cpuGpuLoadChart.getPointX(50), CHART1_Y + CHART_HEIGHT + 5);
  // tft.drawString("100", cpuGpuLoadChart.getPointX(100), CHART1_Y + CHART_HEIGHT + 5);
  tft.drawString("0", CHART1_X - 15, cpuGpuLoadChart.getPointY(0));
  tft.drawString("50", CHART1_X - 15, cpuGpuLoadChart.getPointY(50));
  tft.drawString("100", CHART1_X - 15, cpuGpuLoadChart.getPointY(100));

  cpuLoadTrace.startTrace(TFT_GREEN);
  gpuLoadTrace.startTrace(TFT_BLUE);

  cpuGpuTempChart.createGraph(CHART_WIDTH, CHART_HEIGHT, tft.color565(5, 5, 5));
  cpuGpuTempChart.setGraphScale(0.0, 100.0, 0.0, 100.0);
  cpuGpuTempChart.setGraphGrid(0.0, 25.0, 0.0, 25.0, TFT_DARKGREY);
  cpuGpuTempChart.drawGraph(CHART2_X, CHART2_Y);

  // Legend for chart 2 (top right of chart)
  tft.setTextSize(1); // Smaller text for legend
  tft.fillCircle(DATA_X, LEGEND_ITEM_Y + LINE_HEIGHT, LEGEND_RADIUS, TFT_RED);
  tft.setTextColor(TFT_RED, BG_COLOR);
  tft.drawString("RAM", DATA_X + LEGEND_TEXT_OFFSET, LEGEND_ITEM_Y + LINE_HEIGHT);
  tft.fillCircle(DATA_X + LEGEND_SPACING, LEGEND_ITEM_Y + LINE_HEIGHT, LEGEND_RADIUS, TFT_ORANGE);
  tft.setTextColor(TFT_ORANGE, BG_COLOR);
  tft.drawString("ESP", DATA_X + LEGEND_SPACING + LEGEND_TEXT_OFFSET, LEGEND_ITEM_Y + LINE_HEIGHT);

  // Axis values for chart 2
  tft.setTextColor(TITLE_COLOR, BG_COLOR); // Axis value color
  tft.drawString("0", cpuGpuTempChart.getPointX(0), CHART2_Y + CHART_HEIGHT + 5);
  tft.drawString("50", cpuGpuTempChart.getPointX(50), CHART2_Y + CHART_HEIGHT + 5);
  tft.drawString("100", cpuGpuTempChart.getPointX(100), CHART2_Y + CHART_HEIGHT + 5);
  tft.drawString("0", CHART2_X - 15, cpuGpuTempChart.getPointY(0));
  tft.drawString("50", CHART2_X - 15, cpuGpuTempChart.getPointY(50));
  tft.drawString("100", CHART2_X - 15, cpuGpuTempChart.getPointY(100));

  cpuTempTrace.startTrace(TFT_RED);
  gpuTempTrace.startTrace(TFT_ORANGE);
}

// 更新 PC 数据
void updatePerformanceData() {
  if (xSemaphoreTake(xPCDataMutex, 10) != pdTRUE)
    return;
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.setTextSize(2);
  if (pcData.valid) {
    // CPU Data
    tft.fillRect(DATA_X + VALUE_OFFSET_X, DATA_Y, VALUE_WIDTH, LINE_HEIGHT, BG_COLOR);
    tft.setTextColor(TFT_GREEN, BG_COLOR);
    tft.drawString(String(pcData.cpuLoad) + "% " + String(pcData.cpuTemp) + "C", DATA_X + VALUE_OFFSET_X, DATA_Y);
    
    // GPU Data
    tft.fillRect(DATA_X + VALUE_OFFSET_X, DATA_Y + LINE_HEIGHT, VALUE_WIDTH, LINE_HEIGHT, BG_COLOR);
    tft.setTextColor(TFT_BLUE, BG_COLOR);
    tft.drawString(String(pcData.gpuLoad) + "% " + String(pcData.gpuTemp) + "C", DATA_X + VALUE_OFFSET_X, DATA_Y + LINE_HEIGHT);

    // RAM Data
    tft.fillRect(DATA_X + VALUE_OFFSET_X, DATA_Y + 2 * LINE_HEIGHT, VALUE_WIDTH, LINE_HEIGHT, BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.drawString(String(pcData.ramLoad, 1) + "%", DATA_X + VALUE_OFFSET_X, DATA_Y + 2 * LINE_HEIGHT);

    static float gx = 0.0;
    cpuLoadTrace.addPoint(gx, pcData.cpuLoad);
    gpuLoadTrace.addPoint(gx, pcData.gpuLoad);
    cpuTempTrace.addPoint(gx, pcData.cpuTemp);
    gpuTempTrace.addPoint(gx, pcData.gpuTemp);
    gx += 1.0;
    if (gx > 100.0) {
      gx = 0.0;
      cpuGpuLoadChart.drawGraph(CHART1_X, CHART1_Y);
      cpuLoadTrace.startTrace(TFT_GREEN);
      gpuLoadTrace.startTrace(TFT_BLUE);
      cpuGpuTempChart.drawGraph(CHART2_X, CHART2_Y);
      cpuTempTrace.startTrace(TFT_RED);
      gpuTempTrace.startTrace(TFT_ORANGE);
    }

  } else {
    tft.fillRect(DATA_X + VALUE_OFFSET_X, DATA_Y, VALUE_WIDTH, 3 * LINE_HEIGHT, BG_COLOR);
    tft.setTextColor(ERROR_COLOR, BG_COLOR);
    tft.setTextSize(2);
    tft.drawString("No Data", DATA_X + VALUE_OFFSET_X, DATA_Y);
  }
  
  // ESP32 Temp
  tft.fillRect(DATA_X + VALUE_OFFSET_X, DATA_Y + 3 * LINE_HEIGHT, VALUE_WIDTH, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(TFT_ORANGE, BG_COLOR);
  tft.setTextSize(2);
  tft.drawString(String(esp32c3_temp, 1) + " C", DATA_X + VALUE_OFFSET_X, DATA_Y + 3 * LINE_HEIGHT);
  
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