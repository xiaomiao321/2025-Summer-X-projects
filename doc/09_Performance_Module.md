# 「跨屏监控」PC性能监视器的实现

## 一、模块概述

对于PC爱好者和游戏玩家来说，实时了解计算机核心部件（CPU、GPU）的负载和温度至关重要。本模块将我们的多功能时钟，变成了一个外置的、专用的PC性能监视器副屏。它通过串口与PC端的一个辅助程序通信，接收性能数据，并以一个信息丰富的仪表盘界面，将PC的实时状态可视化地呈现出来。

该模块能显示的数据包括：
-   CPU和GPU的型号名称。
-   CPU和GPU的实时负载（百分比）。
-   CPU和GPU的实时温度（摄氏度）。
-   内存（RAM）的实时使用率。
-   ESP32芯片自身的温度。

除了数字显示，界面下方还有一个动态的折线图，可以实时绘制CPU负载、GPU负载、内存使用率和GPU温度四条数据曲线，让用户能直观地看到性能随时间变化的趋势。

## 二、实现思路与技术选型

### 数据通信：串口协议与数据解析

本项目最核心的设计，在于它如何从PC获取数据。我们选择了最简单、最通用、最可靠的通信方式——**USB串口通信**。

-   **PC端辅助程序**：需要在PC上运行一个脚本（如Python脚本，使用`psutil`和`py-nvml-client`等库）来定时获取硬件信息，然后按照一个我们自定义的、简单的字符串协议，将数据通过串口发送给ESP32设备。
-   **自定义协议**：为了便于解析，我们设计了一个简单的文本协议。例如，PC端发送的字符串可能长这样：`CPU:Intel Core i9|GPU:NVIDIA RTX 4090|C10c30%|G25c60%|RL55.5|
`。其中`C`代表CPU，后面跟着温度和负载；`G`代表GPU；`RL`代表RAM Load。每个数据段用`|`分隔。
-   **ESP32端解析**：在`performance.cpp`中，我们创建了一个`SERIAL_Task`后台任务，专门负责监听串口数据。当它接收到以换行符`\n`结尾的完整数据字符串后，会调用`parsePCData()`函数。这个函数通过一系列的`strstr()`（查找子字符串）和`strncpy()`（拷贝字符串）操作，像做“完形填空”一样，从这个大字符串中精确地提取出各个性能指标的值，并存入一个全局的`PCData`结构体中。这种自定义文本协议的方式，虽然不如JSON或Protobuf等标准化格式强大，但对于这个特定场景，它无需任何外部库，实现简单，性能开销极低。

### 线程安全：互斥锁（Mutex）的应用

我们有多个任务需要访问同一个数据资源：`SERIAL_Task`负责写入最新的`PCData`，而`Performance_Task`负责读取`PCData`并更新屏幕。这种“多写一读”或“多读多写”的场景，如果不加保护，可能会导致数据竞争：比如UI任务正在读取一个数据结构的前半部分，此时串口任务突然写入了新的数据，UI任务再读取后半部分时，就会得到一个“缝合”起来的、毫无意义的错误数据。

为了解决这个问题，我们引入了FreeRTOS的**互斥锁（Mutex）**`xPCDataMutex`。任何任务在访问`pcData`结构体之前，都必须先“获取”这个锁 (`xSemaphoreTake`)。如果锁已被其他任务持有，则当前任务会等待，直到那个任务“释放”锁 (`xSemaphoreGive`)。通过这种机制，我们保证了在任何时刻，只有一个任务能操作`pcData`，从而确保了数据的完整性和一致性。这是多任务编程中保证线程安全的基石。(要系统学习FreeRTOS中的任务同步，可以参考 [FreeRTOS官方文档](https://www.freertos.org/Embedded-RTOS-Queues-Semaphores-Mutexes.html))

### 数据可视化：`TFT_eWidget`图表库

为了绘制动态的性能曲线图，我们使用了作者开发的`TFT_eWidget`库。这个库是`TFT_eSPI`的一个扩展，专门用于创建UI小部件，如按钮、滑块和图表。

-   **图表创建**：我们创建了一个`GraphWidget`对象作为图表的画布，并在其上创建了四个`TraceWidget`对象，分别代表四条不同颜色的数据曲线。
-   **数据更新**：在`Performance_Task`中，每当获取到新的PC数据，我们就会调用`cpuLoadTrace.addPoint(gx, pcData.cpuLoad)`等函数，将新的数据点添加到对应的曲线中。`TFT_eWidget`库会自动处理坐标的映射和线条的绘制。当X轴（代表时间）走完一个周期后，我们只需重新绘制图表背景，并重置曲线，即可开始新一轮的绘制，形成循环滚动的效果。

## 三、代码深度解读

### 串口数据解析 (`performance.cpp`)

```cpp
void parsePCData() {
    struct PCData newData = { ... }; // 创建一个临时结构体
    char *ptr = nullptr;

    // 解析CPU数据
    ptr = strstr(inputBuffer, "C");
    if (ptr) {
        char *degree = strchr(ptr, 'c');
        char *limit = strchr(ptr, '%');
        if (degree && limit && degree < limit) {
            char temp[8] = {0}, load[8] = {0};
            strncpy(temp, ptr + 1, degree - ptr - 1); // 提取温度字符串
            strncpy(load, degree + 2, limit - degree - 2); // 提取负载字符串
            newData.cpuTemp = atoi(temp); // 转为整数
            newData.cpuLoad = atoi(load);
        }
    }
    // ... 类似逻辑解析GPU和RAM ...

    // 使用互斥锁安全地更新全局数据
    if (xSemaphoreTake(xPCDataMutex, portMAX_DELAY) == pdTRUE) {
        pcData = newData;
        xSemaphoreGive(xPCDataMutex);
    }
}
```

`parsePCData`是自定义协议解析的核心。它不依赖复杂的正则表达式，而是通过C语言标准库中的字符串处理函数，一步步地定位和提取信息。例如，为了解析CPU数据，它首先找到标志性的`C`，然后在这个基础上分别找到温度单位`c`和负载单位`%`，这两个字符之间的内容就是需要的数据。`strncpy`用于安全地拷贝出这些子字符串，`atoi`和`atof`则将它们从字符串转换为数值。最后，在所有解析完成后，它通过获取互斥锁，将解析出的`newData`安全地赋值给全局的`pcData`结构体。

### 多任务的创建与管理 (`performance.cpp`)

```cpp
void performanceMenu() {
    // ...
    xPCDataMutex = xSemaphoreCreateMutex(); // 创建互斥锁
    // 创建并启动三个并行的任务
    xTaskCreatePinnedToCore(Performance_Init_Task, "Perf_Init", 8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(Performance_Task, "Perf_Show", 8192, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(SERIAL_Task, "Serial_Rx", 2048, NULL, 1, NULL, 0);

    while (1) { // UI主线程阻塞在这里，等待退出信号
        if (exitSubMenu) {
            // 删除任务和互斥锁，释放资源
            vTaskDelete(xTaskGetHandle("Perf_Show"));
            vTaskDelete(xTaskGetHandle("Serial_Rx"));
            vSemaphoreDelete(xPCDataMutex);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

这是性能监控模块的入口函数，也是多任务管理的“指挥中心”。它首先创建了任务间通信所需的互斥锁，然后使用`xTaskCreatePinnedToCore`启动了三个任务：一个用于初始化UI，一个用于接收串口数据，一个用于刷新屏幕。之后，主函数就进入一个`while`循环， фактически“暂停”在这里，等待用户按下退出按钮。一旦检测到退出信号，它会负责地调用`vTaskDelete`和`vSemaphoreDelete`来销毁之前创建的任务和互斥锁，回收所有系统资源，避免内存泄漏。这是一个完整的、生命周期管理清晰的多任务应用范例。

## 四、效果展示与体验优化

最终的性能监视器界面信息密集但布局清晰，用户可以同时关注到CPU、GPU、RAM和ESP32自身的核心指标。底部的动态曲线图非常直观地反映了PC性能的波动趋势，对于观察游戏或编译过程中的负载变化尤其有用。由于数据解析和UI刷新在不同的任务中并行执行，即便是串口数据快速涌入，UI界面也依然保持流畅。

**[此处应有实际效果图，展示性能监控的完整界面和动态变化的曲线图]**

## 五、总结与展望

性能监视器模块是一个典型的物联网（IoT）应用雏形，它通过“设备（ESP32）+云端/上位机（PC）”的模式，将数据显示在了一个便捷的外部屏幕上。它综合运用了**串口通信、自定义协议、多任务编程和线程安全**等关键的嵌入式和物联网技术。这个模块的实现，标志着我们的多功能时钟已经超越了一个简单的“时钟”，成为一个具备数据交互能力的智能终端。

未来的可扩展方向可以是：
-   **无线化**：将通信方式从有线的串口，升级为蓝牙或WiFi。PC端通过网络将数据发送给ESP32，摆脱线缆的束缚。
-   **更丰富的数据**：除了CPU/GPU，还可以监控网络速度、硬盘读写、风扇转速等更多维度的信息。
-   **配置化界面**：允许用户自定义显示哪些数据，或者调整图表中曲线的颜色和组合。
