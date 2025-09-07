# 温度模块：DS18B20温度监测与历史曲线

## 一、引言

温度模块利用DS18B20数字温度传感器，实时监测环境温度，并通过图形化界面展示当前温度值和历史温度变化曲线。本模块旨在提供一个精确、直观的温度监测解决方案，帮助用户了解环境温度趋势。

## 二、实现思路与技术选型

### 2.1 DS18B20传感器通信

- **OneWire协议**: DS18B20传感器采用单总线（OneWire）协议进行通信，只需要一根数据线即可与微控制器进行数据交换。
- **库支持**: 模块使用了 `OneWire` 库和 `DallasTemperature` 库，这两个库封装了DS18B20传感器的底层通信细节，简化了温度读取过程。

### 2.2 FreeRTOS多任务并发

为了实现温度数据的周期性更新和UI的流畅显示，模块利用FreeRTOS创建了独立的任务：
- `updateTempTask`: 这是一个后台任务，负责每隔一段时间（例如2秒）向DS18B20传感器请求温度数据，并更新 `currentTemperature` 全局变量。这个任务确保了温度数据的实时性。
- `DS18B20_Task`: 这是一个UI任务，负责绘制温度显示界面和历史温度曲线。它会定期从 `currentTemperature` 获取最新温度，并将其添加到图表中。

### 2.3 用户界面与图形化展示

模块使用 `TFT_eSPI` 和 `TFT_eWidget` 库来构建丰富的图形化界面：
- **当前温度显示**: 屏幕上方以大字体显示当前温度值，并带有单位“C”。
- **历史温度曲线**: 使用 `GraphWidget` 和 `TraceWidget` 绘制温度随时间变化的折线图。图表具有X轴（时间/数据点）和Y轴（温度）刻度，并显示网格线。
- **错误提示**: 当传感器连接异常或读取失败时，屏幕会显示“Sensor Error!”的提示信息。

### 2.4 错误处理与退出机制

- **传感器错误检测**: `getTempCByIndex(0)` 返回 `DEVICE_DISCONNECTED_C` 时，表示传感器读取失败。
- **任务停止标志**: `stopDS18B20Task` 标志用于控制 `DS18B20_Task` 的停止。当用户退出菜单或闹钟响铃时，此标志会被设置为 `true`，任务会自行清理并删除。

### 2.5 与其他模块的集成

- **实时时间显示**: 顶部实时时间显示依赖于 `weather.h` 中提供的 `timeinfo` 结构体和 `getLocalTime` 函数。
- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，温度模块能够及时退出，避免功能冲突。

## 三、代码展示

### `DS18B20.cpp`

```c++
#include "DS18B20.h"
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include "MQTT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include "weather.h"
// Graph dimensions and position
#define TEMP_GRAPH_WIDTH  200
#define TEMP_GRAPH_HEIGHT 135
#define TEMP_GRAPH_X      20
#define TEMP_GRAPH_Y      90 

// Temperature display position
#define TEMP_VALUE_X      20
#define TEMP_VALUE_Y      20

// Status message position
#define STATUS_MESSAGE_X  10
#define STATUS_MESSAGE_Y  230 // Adjust this to be below the graph

#define DS18B20_PIN 10


OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

float currentTemperature = -127.0;
bool stopDS18B20Task = false;

GraphWidget gr = GraphWidget(&tft);
TraceWidget tr = TraceWidget(&gr);

void DS18B20_Init() {
  sensors.begin();
}

void updateTempTask(void *pvParameters) {
    while(1) {
        sensors.requestTemperatures();
        float temp = sensors.getTempCByIndex(0);
        if (temp != DEVICE_DISCONNECTED_C) {
            currentTemperature = temp;
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Update every 2 seconds
    }
}

void createDS18B20Task() {
    xTaskCreate(
        updateTempTask,
        "DS18B20 Update Task",
        1024,
        NULL,
        1,
        NULL
    );
}

float getDS18B20Temp() {
    return currentTemperature;
}

void DS18B20_Task(void *pvParameters) {
  float lastTemp = -274;
  float gx = 0.0;

  gr.createGraph(TEMP_GRAPH_WIDTH, TEMP_GRAPH_HEIGHT, tft.color565(5, 5, 5));
  gr.setGraphScale(0.0, 100.0, 0.0, 40.0); // X-axis 0-100, Y-axis 0-40 (temperature range)
  gr.setGraphGrid(0.0, 25.0, 0.0, 10.0, TFT_DARKGREY); // Grid every 25 on X, 10 on Y
  gr.drawGraph(TEMP_GRAPH_X, TEMP_GRAPH_Y);
  tr.startTrace(TFT_YELLOW);

  // Draw Y-axis labels
  tft.setTextSize(1); // Set text size for axis labels
  tft.setTextDatum(MR_DATUM); // Middle-Right datum
  tft.setTextColor(TFT_WHITE, tft.color565(5, 5, 5)); // Set text color for axis labels
  tft.drawNumber(40, gr.getPointX(0.0) - 5, gr.getPointY(40.0));
  tft.drawNumber(30, gr.getPointX(0.0) - 5, gr.getPointY(30.0));
  tft.drawNumber(20, gr.getPointX(0.0) - 5, gr.getPointY(20.0));
  tft.drawNumber(10, gr.getPointX(0.0) - 5, gr.getPointY(10.0));
  tft.drawNumber(0, gr.getPointX(0.0) - 5, gr.getPointY(0.0));

  // Draw X-axis labels
  tft.setTextDatum(TC_DATUM); // Top-Center datum
  tft.drawNumber(0, gr.getPointX(0.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(25, gr.getPointX(25.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(50, gr.getPointX(50.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(75, gr.getPointX(75.0), gr.getPointY(0.0) + 5);
  tft.drawNumber(100, gr.getPointX(100.0), gr.getPointY(0.0) + 5);

  while (1) {
    if (stopDS18B20Task) {
      break;
    }

    float tempC = getDS18B20Temp();

    if (tempC != DEVICE_DISCONNECTED_C && tempC > -50 && tempC < 150) {
      tft.fillRect(0, 0, tft.width(), TEMP_GRAPH_Y - 5, TFT_BLACK); // Clear area above graph
      
      // Display current time at the top
      if (!getLocalTime(&timeinfo)) {
          // Handle error or display placeholder
      } else {
          char time_str[30]; // Increased buffer size
          strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %a", &timeinfo); // New format
          tft.setTextFont(2); // Smaller font for time
          tft.setTextSize(1);
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.setTextDatum(MC_DATUM); // Center align
          tft.drawString(time_str, tft.width() / 2, 10); // Position at top center
          tft.setTextDatum(TL_DATUM); // Reset datum
      }

      char tempStr[10];
      dtostrf(tempC, 4, 2, tempStr);
      
      tft.setTextFont(7); // Set font to 7
      tft.setTextSize(1); // Set size to 1

      // Calculate position to center the text
      // Need to combine tempStr and " C" for accurate width calculation
      char fullTempStr[15];
      sprintf(fullTempStr, "%s C", tempStr);

      int text_width = tft.textWidth(fullTempStr);
      int text_height = tft.fontHeight();
      int x_pos = (tft.width() - text_width) / 2;
      // Adjust y_pos to account for the time display at the top
      int y_pos = (TEMP_GRAPH_Y - 5 - text_height) / 2 + 20; // Shift down by ~20 pixels for time

      tft.setTextDatum(TL_DATUM); // Set to Top-Left for precise positioning
      tft.setTextColor(TFT_WHITE, TFT_BLACK); // Ensure text color is white on black background
      tft.drawString(fullTempStr, x_pos, y_pos);

      tr.addPoint(gx, tempC - 0.5);
      tr.addPoint(gx, tempC);
      tr.addPoint(gx, tempC + 0.5);
      gx += 1.0;

      if (gx > 100.0) {
        gx = 0.0;
        gr.drawGraph(TEMP_GRAPH_X, TEMP_GRAPH_Y);
        tr.startTrace(TFT_YELLOW);
      }

      lastTemp = tempC;
    } else {
      if (stopDS18B20Task) break;

      tft.fillRect(0, 0, tft.width(), tft.height(), TFT_BLACK); // Clear entire screen on error
      tft.setCursor(10, 30);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED);
      tft.println("Sensor Error!");
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  vTaskDelete(NULL);
}

void DS18B20Menu() {
  stopDS18B20Task = false;

  tft.fillScreen(TFT_BLACK);

  xTaskCreatePinnedToCore(DS18B20_Task, "DS18B20_Task", 4096, NULL, 1, NULL, 0);

  while (1) {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        stopDS18B20Task = true;
        vTaskDelay(pdMS_TO_TICKS(150)); // Wait for task to stop
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        stopDS18B20Task = true; // Signal task to stop
        vTaskDelay(pdMS_TO_TICKS(150)); // Give task time to stop
        break; // Exit loop
    }
    if (readButton()) {
      stopDS18B20Task = true;
      TaskHandle_t task = xTaskGetHandle("DS18B20_Task");
      for (int i = 0; i < 150 && task != NULL; i++) {
        if (eTaskGetState(task) == eDeleted) break;
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
```

## 四、代码解读

### 4.1 传感器初始化与数据获取

```c++
#define DS18B20_PIN 10

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

float currentTemperature = -127.0;

void DS18B20_Init() {
  sensors.begin();
}

void updateTempTask(void *pvParameters) {
    while(1) {
        sensors.requestTemperatures();
        float temp = sensors.getTempCByIndex(0);
        if (temp != DEVICE_DISCONNECTED_C) {
            currentTemperature = temp;
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Update every 2 seconds
    }
}
```
- `DS18B20_PIN`: 定义DS18B20传感器连接的GPIO引脚。
- `OneWire` 和 `DallasTemperature` 对象: 用于与DS18B20传感器通信。
- `currentTemperature`: 全局变量，存储最新读取到的温度值。
- `DS18B20_Init()`: 初始化传感器库。
- `updateTempTask()`: 这是一个FreeRTOS任务，每2秒请求一次温度数据，并更新 `currentTemperature`。如果读取失败，`currentTemperature` 保持不变。

### 4.2 温度显示与图表绘制任务 `DS18B20_Task()`

```c++
void DS18B20_Task(void *pvParameters) {
  float lastTemp = -274;
  float gx = 0.0;

  gr.createGraph(TEMP_GRAPH_WIDTH, TEMP_GRAPH_HEIGHT, tft.color565(5, 5, 5));
  gr.setGraphScale(0.0, 100.0, 0.0, 40.0); // X-axis 0-100, Y-axis 0-40 (temperature range)
  gr.setGraphGrid(0.0, 25.0, 0.0, 10.0, TFT_DARKGREY); // Grid every 25 on X, 10 on Y
  gr.drawGraph(TEMP_GRAPH_X, TEMP_GRAPH_Y);
  tr.startTrace(TFT_YELLOW);

  // Draw Y-axis labels
  // ...
  // Draw X-axis labels
  // ...

  while (1) {
    if (stopDS18B20Task) {
      break;
    }

    float tempC = getDS18B20Temp();

    if (tempC != DEVICE_DISCONNECTED_C && tempC > -50 && tempC < 150) {
      tft.fillRect(0, 0, tft.width(), TEMP_GRAPH_Y - 5, TFT_BLACK); // Clear area above graph
      
      // Display current time at the top
      // ...

      char tempStr[10];
      dtostrf(tempC, 4, 2, tempStr);
      
      tft.setTextFont(7); // Set font to 7
      tft.setTextSize(1); // Set size to 1

      // Calculate position to center the text
      // ...

      tft.drawString(fullTempStr, x_pos, y_pos);

      tr.addPoint(gx, tempC - 0.5);
      tr.addPoint(gx, tempC);
      tr.addPoint(gx, tempC + 0.5);
      gx += 1.0;

      if (gx > 100.0) {
        gx = 0.0;
        gr.drawGraph(TEMP_GRAPH_X, TEMP_GRAPH_Y);
        tr.startTrace(TFT_YELLOW);
      }

      lastTemp = tempC;
    } else {
      if (stopDS18B20Task) break;

      tft.fillRect(0, 0, tft.width(), tft.height(), TFT_BLACK); // Clear entire screen on error
      tft.setCursor(10, 30);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED);
      tft.println("Sensor Error!");
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  vTaskDelete(NULL);
}
```
- 这是一个FreeRTOS任务，负责UI的绘制和更新。
- **图表初始化**: 使用 `GraphWidget` 和 `TraceWidget` 初始化温度曲线图，设置坐标轴范围、网格和初始轨迹。
- **循环更新**: 
    - 每500毫秒获取一次最新温度。
    - 如果温度有效，则清空顶部区域，显示当前时间，然后以大字体居中显示当前温度。
    - 将当前温度添加到 `tr` (TraceWidget) 中，绘制到图表上。`gx` 变量控制X轴的滚动，当达到100时，图表会重绘并从头开始。
    - 如果温度无效（`DEVICE_DISCONNECTED_C`），则显示“Sensor Error!”。
- **任务停止**: 检查 `stopDS18B20Task` 标志，如果为 `true`，则退出循环并自行删除任务。

### 4.3 主温度菜单函数 `DS18B20Menu()`

```c++
void DS18B20Menu() {
  stopDS18B20Task = false;

  tft.fillScreen(TFT_BLACK);

  xTaskCreatePinnedToCore(DS18B20_Task, "DS18B20_Task", 4096, NULL, 1, NULL, 0);

  while (1) {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        stopDS18B20Task = true;
        vTaskDelay(pdMS_TO_TICKS(150)); // Wait for task to stop
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        stopDS18B20Task = true; // Signal task to stop
        vTaskDelay(pdMS_TO_TICKS(150)); // Give task time to stop
        break; // Exit loop
    }
    if (readButton()) {
      stopDS18B20Task = true;
      TaskHandle_t task = xTaskGetHandle("DS18B20_Task");
      for (int i = 0; i < 150 && task != NULL; i++) {
        if (eTaskGetState(task) == eDeleted) break;
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
```
- `DS18B20Menu()` 是温度模块的主入口函数。
- **初始化**: 清屏，重置 `stopDS18B20Task` 标志。
- **创建任务**: 创建 `DS18B20_Task` 任务来处理温度显示和图表绘制。
- **主循环**: 持续运行，直到满足退出条件。
- **退出条件**: 
    - `exitSubMenu`: 由主菜单触发的退出标志。
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `readButton()`: 用户单击按键退出。
- **任务停止**: 退出时，将 `stopDS18B20Task` 设置为 `true`，并等待 `DS18B20_Task` 自行删除，确保资源被正确释放。

## 五、总结与展望

温度模块成功集成了DS18B20温度传感器，并通过FreeRTOS多任务和图形化界面，为用户提供了实时、直观的温度监测功能。历史曲线的展示有助于用户了解温度变化趋势。

未来的改进方向：
1.  **多传感器支持**: 允许连接多个DS18B20传感器，并在UI上切换显示不同传感器的温度。
2.  **温度警报**: 设置温度阈值，当温度超出范围时触发视觉或声音警报。
3.  **数据记录与导出**: 将温度数据记录到SD卡或通过网络发送到服务器，方便长期监测和分析。
4.  **更丰富的图表**: 增加更多图表类型，例如日/周/月温度统计图。
5.  **单位切换**: 允许用户在摄氏度和华氏度之间切换温度单位。

谢谢大家
