# 「毫秒必争」高精度秒表的设计与实现

## 一、模块概述

秒表是计时工具中最纯粹、最考验精度的一员。本模块的目标是实现一个专业、精准、响应迅速的数字秒表。它不仅能完成基本的计时功能，还提供了暂停、继续等必要的控制，并能在用户退出后自动清零，确保每次使用的都是一次全新的计时。

该秒表的核心功能是精确到百分之一秒（0.01s）的计时。用户通过单击按钮来控制计时的“开始”与“暂停”，通过长按按钮来“清零并退出”。整个界面设计简洁，将视觉焦点完全集中在跳动的数字上，为用户提供了一个无干扰的计时环境。

## 二、实现思路与技术选型

### 核心逻辑：`millis()`与状态累加

与倒计时模块一样，秒表功能也完全基于`millis()`函数返回的毫秒级时间戳来构建，以保证其精度和独立性。我们定义了几个关键的状态变量来驱动整个逻辑：

-   `stopwatch_running` (bool): 标记秒表当前是否在运行。
-   `stopwatch_start_time` (unsigned long): 记录最近一次“开始”或“继续”计时操作的`millis()`时间戳。
-   `stopwatch_elapsed_time` (unsigned long): 记录在“暂停”之前，已经累积的总时长。

这个模型的核心在于**“分段累加”**的思想：
1.  **开始**：当首次启动时，`stopwatch_running`设为`true`，并记录下`stopwatch_start_time`。
2.  **暂停**：当用户暂停时，我们将本次运行时长（`millis() - stopwatch_start_time`）累加到`stopwatch_elapsed_time`中，然后将`stopwatch_running`设为`false`。
3.  **继续**：当用户从暂停状态恢复时，我们只需再次将`stopwatch_running`设为`true`，并更新`stopwatch_start_time`为当前的`millis()`值即可。

在任何时刻，秒表显示的总时长，就是“已暂停累积的总时长”加上“当前这段正在运行的时长”，即 `stopwatch_elapsed_time + (millis() - stopwatch_start_time)`（仅在运行时计算后半段）。这个模型完美地处理了多次暂停和继续的场景。

### UI设计：动态居中的大字体

为了让用户能清晰地读取时间，我们使用了`TFT_eSPI`库中包含的7段数码管风格的大字体（Font 7）。但这种字体不是等宽的，数字“1”和“8”的宽度差异很大，如果采用固定坐标绘制，会导致数字跳动时整体左右晃动，影响观感。

为了解决这个问题，我们在绘制每一帧时，都会**动态计算**整个时间字符串（如`MM:SS.hh`）的总像素宽度，然后用屏幕总宽度减去这个值再除以2，从而得到让字符串完美居中的起始X坐标。这样，无论数字如何变化，整个时间显示区域的中心线始终与屏幕中心线对齐，提供了稳定、专业的视觉效果。

## 三、代码深度解读

### 核心计时逻辑 (`Stopwatch.cpp`)

```cpp
// Handle single click for start/pause
if (readButton()) {
    if (stopwatch_running) { // Currently running, so pause
        stopwatch_elapsed_time += (millis() - stopwatch_start_time);
        stopwatch_running = false;
    } else { // Currently paused or stopped, so start/resume
        stopwatch_start_time = millis(); // Adjust start time for resume
        stopwatch_running = true;
    }
}

// Update stopwatch display (high frequency)
unsigned long current_stopwatch_display_millis;
if (stopwatch_running) {
    current_stopwatch_display_millis = stopwatch_elapsed_time + (millis() - stopwatch_start_time);
} else {
    current_stopwatch_display_millis = stopwatch_elapsed_time;
}
displayStopwatchTime(current_stopwatch_display_millis);
```

这段代码清晰地展示了“分段累加”模型的实现。当按钮被按下：
-   如果秒表正在运行，就计算从上次开始到现在的增量时长，并将其“固化”到`stopwatch_elapsed_time`中，然后停止运行。
-   如果秒表处于停止状态，就将`stopwatch_start_time`更新为当前时刻，并标记为开始运行。`stopwatch_elapsed_time`中保存的“历史成绩”则保持不变。

在下方的显示逻辑中，根据`stopwatch_running`的状态，选择不同的公式来计算当前应该显示的总时间。这个判断是整个模块逻辑的核心，它确保了无论用户如何操作，时间的计算都是连续和准确的。

### 动态居中布局 (`Stopwatch.cpp`)

```cpp
void displayStopwatchTime(unsigned long elapsed_millis) {
    // ... 时间格式化 ...

    // Font settings for time display
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);

    // Character width calculation for dynamic positioning
    int num_w = menuSprite.textWidth("8"); // Use a wide character for consistent spacing
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");

    // Calculate total width for "MM:SS.hh"
    int total_width = (num_w * 4) + colon_w + dot_w + (num_w * 2);

    // Calculate centered starting position
    int current_x = (menuSprite.width() - total_width) / 2;
    int y_pos = (menuSprite.height() / 2) - (menuSprite.fontHeight() / 2) - 20;

    // ... 逐个绘制字符 ...
}
```

这是解决非等宽字体布局问题的关键。在绘制之前，我们并没有直接使用`setTextDatum(MC_DATUM)`来让整个字符串居中，因为这在某些库的实现中可能仍然会导致微小的晃动。我们采取了更精确的手动计算方法。

我们先获取了数字、冒号和点号的像素宽度，然后根据`MM:SS.hh`这个格式，精确地计算出整个字符串将会占用的总宽度`total_width`。有了这个总宽度，`current_x = (menuSprite.width() - total_width) / 2`就能计算出让这段文本在屏幕上水平居中的精确起始X坐标。接下来，我们从这个`current_x`开始，逐个绘制分钟、冒号、秒钟等，每绘制一个部分，就将`current_x`向右移动相应的像素宽度。这种手动布局的方式虽然稍显繁琐，但能实现最稳定、最精确的对齐效果。

## 四、效果展示与体验优化

最终的秒表模块响应灵敏，计时精准。大号的数码管字体和稳定的居中布局提供了极佳的可读性。交互逻辑简单明了：单击控制启停，长按复位退出，符合大多数用户对秒表工具的心理预期。高频的界面刷新（每10毫秒检查一次时间变化）保证了百分之一秒的数字跳动也清晰可见，充满了动感。

**[此处应有实际效果图，展示秒表在准备、运行和暂停状态下的界面]**

## 五、总结与展望

秒表模块是`millis()`计时应用的又一个经典案例。通过巧妙的状态管理和“分段累加”的计时模型，我们实现了一个功能完备且精度可靠的工具。在UI层面，通过动态计算字符串宽度实现稳定居中的方法，是处理非等宽字体时一个非常实用的技巧。

未来的可扩展方向可以是：
-   **计次（Lap）功能**：这是专业秒表的核心功能。可以增加一个按钮或交互，在秒表运行时记录下当前时间点，并显示一个计次列表。
-   **分段计时（Split Time）**：与计次功能类似，但显示的是自上一次计次以来的时间间隔。
-   **数据导出**：对于长期的计时记录，可以考虑将其保存到SD卡或通过蓝牙/WiFi发送出去。