# 性能监控模块：PC与ESP32状态一览

## 一、引言

性能监控模块旨在为用户提供一个直观的界面，实时显示连接PC的CPU、GPU、内存使用情况以及ESP32自身的内部温度。通过串口通信接收PC数据，并利用图形化界面展示各项指标的动态变化，帮助用户随时掌握系统运行状态。

## 二、实现思路与技术选型

### 2.1 数据获取

- **PC性能数据**: 通过串口接收来自PC端应用程序发送的性能数据。PC端应用程序（例如，一个Python脚本）会定期收集CPU负载、温度、GPU负载、温度和内存使用率，并通过串口发送给ESP32。
- **ESP32内部温度**: 利用ESP32内置的温度传感器，通过 `temperatureRead()` 函数直接获取芯片内部温度。

### 2.2 数据解析与存储

- **串口数据解析**: `SERIAL_Task` 任务负责监听串口数据。当接收到完整的数据包（以换行符结束）时，`parsePCData()` 函数会被调用，它会解析字符串，提取出CPU温度、负载、GPU温度、负载和内存负载等信息。
- **数据结构**: `PCData` 结构体用于存储解析后的PC性能数据，包括CPU名称、GPU名称、各项温度和负载。一个布尔变量 `valid` 用于指示数据是否有效。
- **线程安全**: 由于 `PCData` 会在 `SERIAL_Task` 和 `Performance_Task` 之间共享，为了避免数据竞争，使用了FreeRTOS的互斥量（Mutex）`xPCDataMutex` 来保护对 `pcData` 结构体的访问。

### 2.3 FreeRTOS多任务并发

模块利用FreeRTOS创建了多个任务，以实现数据的异步处理和UI的流畅更新：
- `SERIAL_Task`: 负责串口数据的接收和解析，运行在单独的任务中，避免阻塞主循环。
- `Performance_Task`: 负责定期读取ESP32内部温度，并调用 `updatePerformanceData()` 更新UI显示。
- `Performance_Init_Task`: 负责绘制UI的静态元素（如背景、标签、图表框架），只运行一次。

### 2.4 用户界面与图形化展示

模块使用 `TFT_eSPI` 和 `TFT_eWidget` 库来构建丰富的图形化界面：
- **静态元素**: `drawPerformanceStaticElements()` 函数负责绘制不变的UI元素，如背景、CPU/GPU/RAM/ESP标签、以及图表区域和图例。
- **动态数据**: `updatePerformanceData()` 函数负责更新实时变化的数值，如各项温度和负载。
- **折线图**: 使用 `GraphWidget` 和 `TraceWidget` 绘制CPU负载、GPU负载、RAM使用和GPU温度的实时折线图，直观地展示性能趋势。

## 三、代码展示

### `performance.cpp`

```c++
#include "Performance.h"
#include <TFT_eWidget.h>
#include "Menu.h"
#include "MQTT.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
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

// Combined chart for all four traces
GraphWidget combinedChart = GraphWidget(&tft);
TraceWidget cpuLoadTrace = TraceWidget(&combinedChart);
TraceWidget gpuLoadTrace = TraceWidget(&combinedChart);
TraceWidget cpuTempTrace = TraceWidget(&combinedChart);
TraceWidget gpuTempTrace = TraceWidget(&combinedChart);

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

  // Combined Chart Setup
  combinedChart.createGraph(COMBINED_CHART_WIDTH, COMBINED_CHART_HEIGHT, tft.color565(5, 5, 5));
  combinedChart.setGraphScale(0.0, 100.0, 0.0, 100.0); // X and Y scale from 0 to 100
  combinedChart.setGraphGrid(0.0, 25.0, 0.0, 25.0, TFT_DARKGREY);
  combinedChart.drawGraph(COMBINED_CHART_X, COMBINED_CHART_Y);
  
  // Legend for combined chart
  tft.setTextSize(1); // Smaller text for legend
  
  // CPU Load Legend
  tft.fillCircle(LEGEND_X_START, LEGEND_Y_POS, LEGEND_RADIUS, TFT_GREEN);
  tft.setTextColor(TFT_GREEN, BG_COLOR);
  tft.drawString("CPU Load", LEGEND_X_START + LEGEND_TEXT_OFFSET, LEGEND_Y_POS);
  
  // GPU Load Legend
  tft.fillCircle(LEGEND_X_START + LEGEND_SPACING_WIDE, LEGEND_Y_POS, LEGEND_RADIUS, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, BG_COLOR);
  tft.drawString("GPU Load", LEGEND_X_START + LEGEND_SPACING_WIDE + LEGEND_TEXT_OFFSET, LEGEND_Y_POS);

  // CPU Temp Legend
  tft.fillCircle(LEGEND_X_START, LEGEND_Y_POS + LEGEND_LINE_HEIGHT, LEGEND_RADIUS, TFT_RED);
  tft.setTextColor(TFT_RED, BG_COLOR);
  tft.drawString("RAM", LEGEND_X_START + LEGEND_TEXT_OFFSET, LEGEND_Y_POS + LEGEND_LINE_HEIGHT);

  // GPU Temp Legend
  tft.fillCircle(LEGEND_X_START + LEGEND_SPACING_WIDE, LEGEND_Y_POS + LEGEND_LINE_HEIGHT, LEGEND_RADIUS, TFT_ORANGE);
  tft.setTextColor(TFT_ORANGE, BG_COLOR);
  tft.drawString("GPU Temp", LEGEND_X_START + LEGEND_SPACING_WIDE + LEGEND_TEXT_OFFSET, LEGEND_Y_POS + LEGEND_LINE_HEIGHT);

  // Axis values for combined chart
  tft.setTextColor(TITLE_COLOR, BG_COLOR); // Axis value color
  tft.drawString("0", COMBINED_CHART_X - 15, combinedChart.getPointY(0));
  tft.drawString("50", COMBINED_CHART_X - 15, combinedChart.getPointY(50));
  tft.drawString("100", COMBINED_CHART_X - 15, combinedChart.getPointY(100));

  cpuLoadTrace.startTrace(TFT_GREEN);
  gpuLoadTrace.startTrace(TFT_BLUE);
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
    cpuTempTrace.addPoint(gx, pcData.ramLoad);
    gpuTempTrace.addPoint(gx, pcData.gpuTemp);
    gx += 1.0;
    if (gx > 100.0) {
      gx = 0.0;
      combinedChart.drawGraph(COMBINED_CHART_X, COMBINED_CHART_Y);
      cpuLoadTrace.startTrace(TFT_GREEN);
      gpuLoadTrace.startTrace(TFT_BLUE);
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
    vTaskDelay(pdMS_TO_TICKS(500)); // Update every second for smoother chart updates
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
  tft.fillScreen(TFT_BLACK); // Clear screen directly
  drawPerformanceStaticElements(); // Draw static elements
  xPCDataMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(Performance_Init_Task, "Perf_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(Performance_Task, "Perf_Show", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(SERIAL_Task, "Serial_Rx", 2048, NULL, 1, NULL, 0);
  while (1) {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        vTaskDelete(xTaskGetHandle("Perf_Show"));
        vTaskDelete(xTaskGetHandle("Serial_Rx"));
        vSemaphoreDelete(xPCDataMutex);
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        break; // Exit loop to perform cleanup
    }
    if (readButton()) {
      vTaskDelete(xTaskGetHandle("Perf_Show"));
      vTaskDelete(xTaskGetHandle("Serial_Rx"));
      vSemaphoreDelete(xPCDataMutex);
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  // display = 48;
  // picture_flag = 0;
  // showMenuConfig();
}
```

## 四、代码解读

### 4.1 全局变量与数据结构

```c++
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
struct PCData pcData = {.cpuName = "Unknown", /* ... */ .valid = false};
char inputBuffer[BUFFER_SIZE];
uint16_t bufferIndex = 0;
bool stringComplete = false;
SemaphoreHandle_t xPCDataMutex = NULL;
extern TFT_eSPI tft;

GraphWidget combinedChart = GraphWidget(&tft);
TraceWidget cpuLoadTrace = TraceWidget(&combinedChart);
TraceWidget gpuLoadTrace = TraceWidget(&combinedChart);
TraceWidget cpuTempTrace = TraceWidget(&combinedChart);
TraceWidget gpuTempTrace = TraceWidget(&combinedChart);
```
- `PCData`: 结构体，用于存储从PC接收到的性能数据，包括CPU/GPU名称、温度、负载和RAM负载。`valid` 字段指示数据是否有效。
- `esp32c3_temp`: 存储ESP32-C3内部温度。
- `inputBuffer`, `bufferIndex`, `stringComplete`: 用于串口数据接收的缓冲区和状态变量。
- `xPCDataMutex`: FreeRTOS互斥量，用于保护 `pcData` 结构体的并发访问。
- `combinedChart`: `TFT_eWidget` 库中的 `GraphWidget` 对象，用于绘制组合图表。
- `cpuLoadTrace`, `gpuLoadTrace`, `cpuTempTrace`, `gpuTempTrace`: `TFT_eWidget` 库中的 `TraceWidget` 对象，用于在 `combinedChart` 上绘制多条折线。

### 4.2 绘制静态UI元素 `drawPerformanceStaticElements()`

```c++
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

  // Combined Chart Setup
  combinedChart.createGraph(COMBINED_CHART_WIDTH, COMBINED_CHART_HEIGHT, tft.color565(5, 5, 5));
  combinedChart.setGraphScale(0.0, 100.0, 0.0, 100.0); // X and Y scale from 0 to 100
  combinedChart.setGraphGrid(0.0, 25.0, 0.0, 25.0, TFT_DARKGREY);
  combinedChart.drawGraph(COMBINED_CHART_X, COMBINED_CHART_Y);
  
  // Legend for combined chart
  tft.setTextSize(1); // Smaller text for legend
  
  // CPU Load Legend
  tft.fillCircle(LEGEND_X_START, LEGEND_Y_POS, LEGEND_RADIUS, TFT_GREEN);
  tft.setTextColor(TFT_GREEN, BG_COLOR);
  tft.drawString("CPU Load", LEGEND_X_START + LEGEND_TEXT_OFFSET, LEGEND_Y_POS);
  
  // GPU Load Legend
  tft.fillCircle(LEGEND_X_START + LEGEND_SPACING_WIDE, LEGEND_Y_POS, LEGEND_RADIUS, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, BG_COLOR);
  tft.drawString("GPU Load", LEGEND_X_START + LEGEND_SPACING_WIDE + LEGEND_TEXT_OFFSET, LEGEND_Y_POS);

  // CPU Temp Legend
  tft.fillCircle(LEGEND_X_START, LEGEND_Y_POS + LEGEND_LINE_HEIGHT, LEGEND_RADIUS, TFT_RED);
  tft.setTextColor(TFT_RED, BG_COLOR);
  tft.drawString("RAM", LEGEND_X_START + LEGEND_TEXT_OFFSET, LEGEND_Y_POS + LEGEND_LINE_HEIGHT);

  // GPU Temp Legend
  tft.fillCircle(LEGEND_X_START + LEGEND_SPACING_WIDE, LEGEND_Y_POS + LEGEND_LINE_HEIGHT, LEGEND_RADIUS, TFT_ORANGE);
  tft.setTextColor(TFT_ORANGE, BG_COLOR);
  tft.drawString("GPU Temp", LEGEND_X_START + LEGEND_SPACING_WIDE + LEGEND_TEXT_OFFSET, LEGEND_Y_POS + LEGEND_LINE_HEIGHT);

  // Axis values for combined chart
  tft.setTextColor(TITLE_COLOR, BG_COLOR); // Axis value color
  tft.drawString("0", COMBINED_CHART_X - 15, combinedChart.getPointY(0));
  tft.drawString("50", COMBINED_CHART_X - 15, combinedChart.getPointY(50));
  tft.drawString("100", COMBINED_CHART_X - 15, combinedChart.getPointY(100));

  cpuLoadTrace.startTrace(TFT_GREEN);
  gpuLoadTrace.startTrace(TFT_BLUE);
  cpuTempTrace.startTrace(TFT_RED);
  gpuTempTrace.startTrace(TFT_ORANGE);
}
```
- 此函数负责绘制性能监控界面的所有静态元素，包括背景、文本标签、图表框架和图例。
- 使用 `tft.pushImage` 绘制NVIDIA和Intel的Logo。
- `combinedChart.createGraph` 设置图表的尺寸和背景色。
- `combinedChart.setGraphScale` 设置图表的X轴和Y轴的范围（0-100）。
- `combinedChart.setGraphGrid` 绘制网格线。
- `TraceWidget.startTrace` 初始化每条折线的颜色。

### 4.3 更新PC数据 `updatePerformanceData()`

```c++
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
    cpuTempTrace.addPoint(gx, pcData.ramLoad);
    gpuTempTrace.addPoint(gx, pcData.gpuTemp);
    gx += 1.0;
    if (gx > 100.0) {
      gx = 0.0;
      combinedChart.drawGraph(COMBINED_CHART_X, COMBINED_CHART_Y);
      cpuLoadTrace.startTrace(TFT_GREEN);
      gpuLoadTrace.startTrace(TFT_BLUE);
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
```
- 此函数负责更新屏幕上显示的PC性能数据和ESP32内部温度。
- 在访问 `pcData` 之前，会尝试获取 `xPCDataMutex` 互斥量，确保数据的一致性。
- 如果 `pcData.valid` 为真，则显示CPU、GPU和RAM的负载和温度数据，并更新图表上的折线。
- `gx` 变量用于控制图表X轴的滚动，当达到100时，图表会重绘并从头开始。
- 如果 `pcData.valid` 为假，则显示“No Data”。
- 最后，显示ESP32的内部温度。

### 4.4 串口数据解析 `parsePCData()`

```c++
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
  if (ptr) { /* ... 解析CPU数据 ... */ }
  ptr = strstr(inputBuffer, "G");
  if (ptr) { /* ... 解析GPU数据 ... */ }
  ptr = strstr(inputBuffer, "RL");
  if (ptr) { /* ... 解析RAM数据 ... */ }
  ptr = strstr(inputBuffer, "CPU:");
  if (ptr) { /* ... 解析CPU名称 ... */ }
  ptr = strstr(inputBuffer, "GPU:");
  if (ptr) { /* ... 解析GPU名称 ... */ }
  if (newData.cpuTemp > 0 || newData.gpuTemp > 0 || newData.ramLoad > 0) {
    newData.valid = true;
  }
  if (xSemaphoreTake(xPCDataMutex, portMAX_DELAY) == pdTRUE) {
    pcData = newData;
    xSemaphoreGive(xPCDataMutex);
  }
}
```
- 此函数负责解析从 `inputBuffer` 中接收到的PC性能数据字符串。
- 它使用 `strstr` 和 `strchr` 等字符串函数查找特定的标识符（如“C”、“G”、“RL”、“CPU:” 、“GPU:”），然后提取相应的数值。
- 解析完成后，将新数据存储到 `newData` 结构体中，并设置 `valid` 标志。
- 最后，在获取互斥量后，将 `newData` 赋值给全局的 `pcData`。

### 4.5 串口接收任务 `SERIAL_Task()`

```c++
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
```
- 这是一个FreeRTOS任务，负责从串口读取数据。
- 它会逐个字符读取串口缓冲区，并存储到 `inputBuffer` 中。
- 当检测到换行符 (`\n`) 时，设置 `stringComplete` 为 `true`，表示接收到一个完整的数据包。
- 如果 `stringComplete` 为真，则调用 `parsePCData()` 解析数据，然后重置缓冲区和标志。

### 4.6 性能监控显示任务 `Performance_Task()`

```c++
void Performance_Task(void *pvParameters) {
  for (;;)
 {
    esp32c3_temp = temperatureRead();
    updatePerformanceData();
    vTaskDelay(pdMS_TO_TICKS(500)); // Update every second for smoother chart updates
  }
}
```
- 这是一个FreeRTOS任务，负责定期更新性能数据。
- 它每500毫秒读取一次ESP32的内部温度，并调用 `updatePerformanceData()` 来刷新UI。

### 4.7 主性能监控菜单函数 `performanceMenu()`

```c++
void performanceMenu() {
  tft.fillScreen(TFT_BLACK); // Clear screen directly
  drawPerformanceStaticElements(); // Draw static elements
  xPCDataMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(Performance_Init_Task, "Perf_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(Performance_Task, "Perf_Show", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(SERIAL_Task, "Serial_Rx", 2048, NULL, 1, NULL, 0);
  while (1) {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        vTaskDelete(xTaskGetHandle("Perf_Show"));
        vTaskDelete(xTaskGetHandle("Serial_Rx"));
        vSemaphoreDelete(xPCDataMutex);
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        break; // Exit loop to perform cleanup
    }
    if (readButton()) {
      vTaskDelete(xTaskGetHandle("Perf_Show"));
      vTaskDelete(xTaskGetHandle("Serial_Rx"));
      vSemaphoreDelete(xPCDataMutex);
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  // display = 48;
  // picture_flag = 0;
  // showMenuConfig();
}
```
- `performanceMenu()` 是性能监控模块的主入口函数。
- **初始化**: 清屏，绘制静态UI元素，创建互斥量，并创建 `Performance_Init_Task`、`Performance_Task` 和 `SERIAL_Task`。
- **主循环**: 持续运行，直到满足退出条件。
- **退出条件**: 
    - `exitSubMenu`: 由主菜单触发的退出标志。
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `readButton()`: 用户单击按键退出。
- **资源清理**: 退出时，会删除所有相关的FreeRTOS任务和互斥量，释放资源。

## 五、总结与展望

性能监控模块通过串口通信和FreeRTOS多任务，实现了PC性能数据和ESP32内部温度的实时显示。图形化的折线图提供了直观的性能趋势，帮助用户更好地了解系统运行状况。

未来的改进方向：
1.  **更多数据点**: 除了CPU、GPU和RAM，可以增加网络带宽、磁盘I/O等更多性能指标的显示。
2.  **网络数据获取**: 探索通过网络（例如，UDP或TCP）而不是串口来获取PC性能数据，以实现无线监控。
3.  **自定义图表**: 允许用户自定义图表的颜色、线条样式和显示范围。
4.  **数据记录与回放**: 增加性能数据的记录功能，并允许用户回放历史数据。
5.  **警报功能**: 当某个性能指标（如温度或负载）超过阈值时，触发视觉或声音警报。

谢谢大家

```