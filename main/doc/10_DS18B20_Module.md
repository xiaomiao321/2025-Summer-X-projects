# 「感知世界」DS18B20温度传感器模块详解

## 一、模块概述

除了从网络获取虚拟的天气数据，我们的多功能时钟还能通过硬件传感器，直接感知其所处环境的真实温度。本模块专门负责与**DS18B20**这款经典的单总线数字温度传感器进行交互，读取精确的环境温度，并以数字和动态曲线图的形式，将物理世界的变化呈现在屏幕上。

该模块的核心功能包括：
1.  通过One-Wire（单总线）协议与DS18B20传感器通信。
2.  定时、自动地在后台读取温度数据。
3.  提供一个专用的显示界面，包含一个大字体的实时温度读数和一条记录历史温度变化的动态曲线图。

## 二、实现思路与技术选型

### 硬件交互：`OneWire`与`DallasTemperature`库

DS18B20传感器使用一种独特的“单总线”协议进行通信，即所有数据交换都通过一根数据线完成。从零开始实现这个协议是相当复杂的。为了简化开发，我们引入了两个在Arduino生态中非常成熟的第三方库：

-   **`OneWire.h`**：这个库提供了与单总线设备进行底层通信所需的基础函数，它处理了总线时序、设备寻址等复杂细节。
-   **`DallasTemperature.h`**：这个库构建在`OneWire`之上，专门为Dallas/Maxim系列的温度传感器（包括DS18B20）提供了高级的、面向用户的API。我们只需调用`sensors.requestTemperatures()`和`sensors.getTempCByIndex(0)`这样简单的函数，就能轻松地获取到摄氏温度读数，而无需关心底层的通信协议和数据转换。这是在嵌入式开发中，通过利用现有库来“站在巨人肩膀上”的典型范例。(要了解更多关于DS18B20及其库的使用，可以参考 [Random Nerd Tutorials上的这篇教程](https://randomnerdtutorials.com/esp32-ds18b20-temperature-sensor-arduino-ide/))

### 数据读取：后台任务与数据平滑

温度是一个缓慢变化的物理量，我们不需要在UI任务中高频地读取它。而且，`sensors.requestTemperatures()`这个函数本身会有一定的延时（等待温度转换完成）。为了不阻塞UI，我们将温度读取的逻辑也放入了一个独立的**后台任务`updateTempTask`**中。

这个任务非常简单：每隔2秒钟，它就向传感器请求一次温度读数，并将获取到的值更新到一个全局变量`currentTemperature`中。UI任务则直接从这个全局变量读取温度值进行显示。这种“生产者-消费者”模式，将耗时的硬件操作与UI刷新彻底解耦，保证了界面的流畅性。

### 数据可视化：`TFT_eWidget`的再次应用

与性能监控模块类似，我们再次使用`TFT_eWidget`库来绘制历史温度曲线图。`GraphWidget`被用来创建图表的坐标系和网格，`TraceWidget`则用来绘制实际的温度曲线。每当UI任务刷新时，它会读取最新的`currentTemperature`值，并调用`tr.addPoint()`将其添加到图表中，直观地展示出温度在过去一段时间内的变化趋势。

## 三、代码深度解读

### 传感器初始化与后台读取 (`DS18B20.cpp`)

```cpp
#define DS18B20_PIN 10

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

float currentTemperature = -127.0;

void DS18B20_Init() {
  sensors.begin();
}

void updateTempTask(void *pvParameters) {
    while(1) {
        sensors.requestTemperatures(); // 发起温度转换请求
        float temp = sensors.getTempCByIndex(0); // 获取温度读数
        if (temp != DEVICE_DISCONNECTED_C) { // 检查返回值是否有效
            currentTemperature = temp;
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // 延时2秒
    }
}
```

这段代码展示了与DS18B20交互的基础流程。`OneWire`和`DallasTemperature`对象被实例化为全局变量。`DS18B20_Init()`函数（应在`setup()`中调用）负责启动传感器。核心是`updateTempTask`任务，它以一个固定的2秒为周期，不断地请求并更新全局的`currentTemperature`变量。`if (temp != DEVICE_DISCONNECTED_C)`这个检查非常重要，`DallasTemperature`库在无法读取到传感器时会返回一个特殊的常量`DEVICE_DISCONNECTED_C`（通常是-127），通过这个检查可以有效地过滤掉无效读数，防止错误数据污染我们的显示。

### UI任务与数据呈现 (`DS18B20.cpp`)

```cpp
void DS18B20_Task(void *pvParameters) {
  // ... 图表初始化 ...
  float gx = 0.0; // X轴的当前位置

  while (1) {
    if (stopDS18B20Task) { break; } // 检查退出标志

    float tempC = getDS18B20Temp(); // 从全局变量获取温度

    if (tempC != DEVICE_DISCONNECTED_C && tempC > -50 && tempC < 150) {
      // ... 清理屏幕和绘制静态文本 ...

      // 绘制大字体的温度读数
      tft.setTextFont(7);
      tft.drawString(fullTempStr, x_pos, y_pos);

      // 将新数据点添加到图表中
      tr.addPoint(gx, tempC);
      gx += 1.0;

      if (gx > 100.0) { // 如果图表画满了
        gx = 0.0; // X轴回到起点
        gr.drawGraph(TEMP_GRAPH_X, TEMP_GRAPH_Y); // 重绘图表背景
        tr.startTrace(TFT_YELLOW); // 重新开始绘制曲线
      }
    } else {
      // ... 显示传感器错误信息 ...
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  vTaskDelete(NULL);
}
```

这是负责显示温度界面的前台任务。它以更高的频率（每500毫秒）运行，以保证UI的动态效果（如图表滚动）足够平滑。它从不直接与传感器交互，而是通过`getDS18B20Temp()`函数简单地获取由后台任务准备好的`currentTemperature`值。这种分离使得UI代码非常干净。在将数据点添加到图表后，它会检查X轴`gx`是否已超出范围，如果是，则通过重绘图表和重置曲线的方式，实现曲线的循环滚动效果。

## 四、效果展示与体验优化

最终的温度监控界面提供了清晰、直观的读数体验。上方的大号数字温度让用户可以从远处快速读取，下方的动态曲线则为观察温度的长期变化提供了有力的工具。将数据采集与UI刷新分离到不同的任务中，保证了即使用户在与其它菜单交互，后台的温度数据采集也从未间断，当用户切回温度界面时，能立刻看到最新的数据和连贯的曲线。

**[此处应有实际效果图，展示温度监控界面，包括大数字和动态曲线图]**

## 五、总结与展望

DS18B20温度模块是一个典型的“硬件驱动”功能。它展示了如何通过使用成熟的第三方库来简化底层硬件交互，以及如何利用多任务架构将耗时的I/O操作与UI刷新解耦，从而实现一个既能精确采集数据、又能流畅显示的应用。这是构建任何涉及硬件传感器项目的通用范式。

未来的可扩展方向可以是：
-   **温湿度传感器**：可以替换为DHT22或SHT30等同时能测量温度和湿度的传感器，在同一个界面上显示两条曲线。
-   **数据记录与预警**：将历史温度数据保存到EEPROM或SD卡中，并允许用户设置高/低温阈值，当温度超出范围时自动发出警报。
-   **多点测温**：DS18B20的单总线协议允许在同一根数据线上挂载多个传感器。我们可以扩展程序，使其能同时读取并显示来自多个探头的温度，用于监测不同位置的温度变化。