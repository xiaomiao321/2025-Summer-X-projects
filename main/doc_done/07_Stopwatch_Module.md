# 秒表模块：精确计时与便捷操作

## 一、引言

秒表模块是多功能时钟中一个基础而实用的工具，它提供精确到百分之一秒的计时功能，适用于各种需要测量时间的应用场景。本模块旨在提供一个直观、易于操作的秒表，支持启动、暂停、恢复和重置功能，并提供清晰的视觉反馈。

## 二、实现思路与技术选型

### 2.1 状态管理

秒表模块通过几个关键变量来管理其状态：
- `stopwatch_running`: 布尔值，指示秒表是否正在计时。
- `stopwatch_start_time`: 记录秒表开始或恢复计时时的 `millis()` 值。
- `stopwatch_elapsed_time`: 记录秒表暂停时累计的已用时间（毫秒）。

这些变量协同工作，确保秒表在不同状态（运行、暂停、重置）之间平滑切换，并准确计算总时长。

### 2.2 时间计算与显示

秒表时间需要精确到百分之一秒，并以分钟、秒和百分之一秒的形式显示。模块会动态计算数字和符号的位置，确保时间始终居中显示，提供良好的可读性。
- `elapsed_millis`: 当前秒表总的已用毫秒数，是所有时间计算的基础。
- 分钟、秒、百分之一秒的提取：通过除法和取模运算从 `elapsed_millis` 中精确计算。
- 动态布局：根据屏幕宽度和字体大小，动态计算数字和冒号、小数点的位置，确保时间始终居中显示。

### 2.3 用户交互

模块主要通过一个按键进行交互：
- **单击**: 
    - 如果秒表正在运行，单击会暂停秒表。
    - 如果秒表已暂停或未启动，单击会启动或恢复秒表。
- **长按**: 重置秒表，并将所有计时数据清零，同时退出秒表模块。

### 2.4 视觉反馈

- **状态文本**: 屏幕下方显示“RUNNING”、“PAUSED”或“READY”等状态文本，清晰指示秒表状态。
- **实时更新**: 秒表显示会高频率更新（每百分之一秒），确保计时的流畅性。

### 2.5 与其他模块的集成

- **实时时间显示**: 顶部实时时间显示依赖于 `weather.h` 中提供的 `timeinfo` 结构体和 `getLocalTime` 函数。
- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，秒表模块能够及时退出，避免功能冲突。

## 三、代码展示

### `Stopwatch.cpp`

```c++
#include "Stopwatch.h"
#include "Menu.h"
#include "MQTT.h"
#include "Buzzer.h"
#include "Alarm.h"
#include "RotaryEncoder.h"
#include "weather.h"
#define LONG_PRESS_DURATION 1500 // milliseconds for long press to exit

// Global variables for stopwatch state
static unsigned long stopwatch_start_time = 0;
static unsigned long stopwatch_elapsed_time = 0;
static bool stopwatch_running = false;
static unsigned long stopwatch_pause_time = 0;

// Function to display the stopwatch time with dynamic layout
// Added current_hold_duration for long press progress bar
void displayStopwatchTime(unsigned long elapsed_millis) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(TL_DATUM); // Use Top-Left for precise positioning

    // Display current time at the top
    if (!getLocalTime(&timeinfo)) {
        // Handle error or display placeholder
    } else {
        char time_str[30]; // Increased buffer size
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %a", &timeinfo); // New format
        menuSprite.setTextFont(2); // Smaller font for time
        menuSprite.setTextSize(1);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextDatum(MC_DATUM); // Center align
        menuSprite.drawString(time_str, menuSprite.width() / 2, 10); // Position at top center
        menuSprite.setTextDatum(TL_DATUM); // Reset datum
    }

    // Time calculation
    unsigned long total_seconds = elapsed_millis / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (elapsed_millis % 1000) / 10;

    // Font settings for time display
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);

    // Character width calculation for dynamic positioning
    int num_w = menuSprite.textWidth("8"); // Use a wide character for consistent spacing
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    int num_h = menuSprite.fontHeight();

    // Calculate total width for "MM:SS.hh"
    int total_width = (num_w * 4) + colon_w + dot_w + (num_w * 2);

    // Calculate centered starting position
    int current_x = (menuSprite.width() - total_width) / 2;
    int y_pos = (menuSprite.height() / 2) - (num_h / 2) - 20;

    char buf[3];

    // Draw Minutes
    sprintf(buf, "%02ld", minutes);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    // Draw Colon
    menuSprite.drawString(":", current_x, y_pos);
    current_x += colon_w;

    // Draw Seconds
    sprintf(buf, "%02ld", seconds);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    // Draw Dot
    menuSprite.drawString(".", current_x, y_pos);
    current_x += dot_w;

    // Draw Hundredths
    sprintf(buf, "%02ld", hundredths);
    menuSprite.drawString(buf, current_x, y_pos);

    // --- Display Status Text ---
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (stopwatch_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (elapsed_millis > 0) {
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else {
        menuSprite.drawString("READY", menuSprite.width() / 2, menuSprite.height() - 80);
    }


    menuSprite.pushSprite(0, 0);
}

// Main Stopwatch Menu function
void StopwatchMenu() {
    // Reset state when entering the menu
    stopwatch_start_time = 0;
    stopwatch_elapsed_time = 0;
    stopwatch_running = false;
    stopwatch_pause_time = 0;
    displayStopwatchTime(0); // Initial display
    static unsigned long last_displayed_stopwatch_millis = 0; // Track last displayed value for smooth updates
    unsigned long last_realtime_clock_update = millis(); // For real-time clock update

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            return; // Exit the StopwatchMenu function
        }
        if (g_alarm_is_ringing) { return; }

        // Handle long press for reset and exit
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100); // Exit sound
            // Reset stopwatch state
            stopwatch_start_time = 0;
            stopwatch_elapsed_time = 0;
            stopwatch_running = false;
            stopwatch_pause_time = 0;
            last_displayed_stopwatch_millis = 0; // Reset this as well
            return; // Exit the StopwatchMenu function
        }

        // Handle single click for start/pause
        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50); // Confirm sound
            if (stopwatch_running) { // Currently running, so pause
                stopwatch_elapsed_time += (millis() - stopwatch_start_time);
                stopwatch_running = false;
            } else { // Currently paused or stopped, so start/resume
                stopwatch_start_time = millis(); // Adjust start time for resume
                stopwatch_running = true;
            }
            // IMMEDIATE DISPLAY UPDATE AFTER STATE CHANGE
            unsigned long current_display_value;
            if (stopwatch_running) {
                current_display_value = stopwatch_elapsed_time + (millis() - stopwatch_start_time);
            } else {
                current_display_value = stopwatch_elapsed_time;
            }
            displayStopwatchTime(current_display_value);
            last_displayed_stopwatch_millis = current_display_value; // Update this immediately
        }

        // Update stopwatch display (high frequency)
        unsigned long current_stopwatch_display_millis;
        if (stopwatch_running) {
            current_stopwatch_display_millis = stopwatch_elapsed_time + (millis() - stopwatch_start_time);
        } else {
            current_stopwatch_display_millis = stopwatch_elapsed_time;
        }
        
        // Update stopwatch display if hundredths changed OR if real-time clock needs update
        // This ensures both stopwatch and real-time clock are updated.
        if (current_stopwatch_display_millis / 10 != last_displayed_stopwatch_millis / 10 || (millis() - last_realtime_clock_update) >= 1000) {
            displayStopwatchTime(current_stopwatch_display_millis);
            last_displayed_stopwatch_millis = current_stopwatch_display_millis; // Update last displayed value
            if ((millis() - last_realtime_clock_update) >= 1000) {
                last_realtime_clock_update = millis(); // Update real-time clock update time
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
}
```

## 四、代码解读

### 4.1 秒表状态变量

```c++
static unsigned long stopwatch_start_time = 0;
static unsigned long stopwatch_elapsed_time = 0;
static bool stopwatch_running = false;
static unsigned long stopwatch_pause_time = 0;
```
- `stopwatch_start_time`: 记录秒表开始或从暂停状态恢复时的 `millis()` 时间戳。
- `stopwatch_elapsed_time`: 记录秒表暂停时，已经累计的计时时长。当秒表运行时，这个值加上 `millis() - stopwatch_start_time` 就是当前的总时长。
- `stopwatch_running`: 布尔标志，指示秒表是否正在运行。
- `stopwatch_pause_time`: 记录秒表暂停时的 `millis()` 时间戳（在当前代码中未直接使用，但通常用于更复杂的暂停/恢复逻辑）。

### 4.2 秒表UI绘制 `displayStopwatchTime()`

```c++
void displayStopwatchTime(unsigned long elapsed_millis) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(TL_DATUM); // Use Top-Left for precise positioning

    // Display current time at the top
    // ... (实时时间显示逻辑)

    // Time calculation
    unsigned long total_seconds = elapsed_millis / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (elapsed_millis % 1000) / 10;

    // Font settings for time display
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);

    // Character width calculation for dynamic positioning
    int num_w = menuSprite.textWidth("8"); // Use a wide character for consistent spacing
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    int num_h = menuSprite.fontHeight();

    // Calculate total width for "MM:SS.hh"
    int total_width = (num_w * 4) + colon_w + dot_w + (num_w * 2);

    // Calculate centered starting position
    int current_x = (menuSprite.width() - total_width) / 2;
    int y_pos = (menuSprite.height() / 2) - (num_h / 2) - 20;

    char buf[3];

    // Draw Minutes
    sprintf(buf, "%02ld", minutes);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    // Draw Colon
    menuSprite.drawString(":", current_x, y_pos);
    current_x += colon_w;

    // Draw Seconds
    sprintf(buf, "%02ld", seconds);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    // Draw Dot
    menuSprite.drawString(".", current_x, y_pos);
    current_x += dot_w;

    // Draw Hundredths
    sprintf(buf, "%02ld", hundredths);
    menuSprite.drawString(buf, current_x, y_pos);

    // --- Display Status Text ---
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (stopwatch_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (elapsed_millis > 0) {
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else {
        menuSprite.drawString("READY", menuSprite.width() / 2, menuSprite.height() - 80);
    }


    menuSprite.pushSprite(0, 0);
}
```
- **实时时间显示**: 顶部显示当前系统时间，与 `weather.cpp` 模块共享 `timeinfo`。
- **时间计算**: 将传入的 `elapsed_millis` 转换为分钟、秒和百分之一秒，用于显示。
- **动态布局**: 计算数字和符号的宽度，以确保整个时间字符串在屏幕上居中显示。
- **状态文本**: 根据 `stopwatch_running` 和 `elapsed_millis` 的值，显示不同的状态提示（如“RUNNING”、“PAUSED”或“READY”）。
- **双缓冲**: 所有绘制操作都在 `menuSprite` 上进行，最后通过 `menuSprite.pushSprite(0, 0)` 推送到屏幕，避免闪烁。

### 4.3 主秒表函数 `StopwatchMenu()`

```c++
void StopwatchMenu() {
    // Reset state when entering the menu
    stopwatch_start_time = 0;
    stopwatch_elapsed_time = 0;
    stopwatch_running = false;
    stopwatch_pause_time = 0;
    displayStopwatchTime(0); // Initial display
    static unsigned long last_displayed_stopwatch_millis = 0; // Track last displayed value for smooth updates
    unsigned long last_realtime_clock_update = millis(); // For real-time clock update

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            return; // Exit the StopwatchMenu function
        }
        if (g_alarm_is_ringing) { return; }

        // Handle long press for reset and exit
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100); // Exit sound
            // Reset stopwatch state
            stopwatch_start_time = 0;
            stopwatch_elapsed_time = 0;
            stopwatch_running = false;
            stopwatch_pause_time = 0;
            last_displayed_stopwatch_millis = 0; // Reset this as well
            return; // Exit the StopwatchMenu function
        }

        // Handle single click for start/pause
        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50); // Confirm sound
            if (stopwatch_running) { // Currently running, so pause
                stopwatch_elapsed_time += (millis() - stopwatch_start_time);
                stopwatch_running = false;
            } else { // Currently paused or stopped, so start/resume
                stopwatch_start_time = millis(); // Adjust start time for resume
                stopwatch_running = true;
            }
            // IMMEDIATE DISPLAY UPDATE AFTER STATE CHANGE
            unsigned long current_display_value;
            if (stopwatch_running) {
                current_display_value = stopwatch_elapsed_time + (millis() - stopwatch_start_time);
            } else {
                current_display_value = stopwatch_elapsed_time;
            }
            displayStopwatchTime(current_display_value);
            last_displayed_stopwatch_millis = current_display_value; // Update this immediately
        }

        // Update stopwatch display (high frequency)
        unsigned long current_stopwatch_display_millis;
        if (stopwatch_running) {
            current_stopwatch_display_millis = stopwatch_elapsed_time + (millis() - stopwatch_start_time);
        } else {
            current_stopwatch_display_millis = stopwatch_elapsed_time;
        }
        
        // Update stopwatch display if hundredths changed OR if real-time clock needs update
        // This ensures both stopwatch and real-time clock are updated.
        if (current_stopwatch_display_millis / 10 != last_displayed_stopwatch_millis / 10 || (millis() - last_realtime_clock_update) >= 1000) {
            displayStopwatchTime(current_stopwatch_display_millis);
            last_displayed_stopwatch_millis = current_stopwatch_display_millis; // Update last displayed value
            if ((millis() - last_realtime_clock_update) >= 1000) {
                last_realtime_clock_update = millis(); // Update real-time clock update time
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
}
```
- **初始化**: 进入 `StopwatchMenu` 时，重置所有秒表状态变量为初始值（秒表停止，时间为0）。
- **主循环**: `while(true)` 循环持续运行，直到满足退出条件。
- **退出条件**: 
    - `exitSubMenu`: 由主菜单触发的退出标志。
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `readButtonLongPress()`: 用户长按按键退出并重置秒表。
- **按键交互**: 
    - 单击 `readButton()`: 
        - 如果秒表正在运行，则暂停秒表，并将当前已用时间累加到 `stopwatch_elapsed_time`。
        - 如果秒表已暂停或未启动，则启动或恢复秒表，并更新 `stopwatch_start_time`。
- **显示更新**: 
    - 秒表显示会高频率更新（每百分之一秒），确保计时的流畅性。通过比较 `current_stopwatch_display_millis / 10` 和 `last_displayed_stopwatch_millis / 10` 来判断百分之一秒是否变化。
    - 顶部的实时时间每秒更新一次。
- **`vTaskDelay`**: 引入短暂延时，避免忙等待，降低CPU负载。

## 五、总结与展望

秒表模块提供了一个精确且易于操作的计时工具。其简洁的交互逻辑和实时的视觉反馈，使其成为多功能时钟中一个实用的功能。

未来的改进方向：
1.  **分段计时（Lap Times）**: 增加记录分段时间的功能，方便用户测量多个阶段的时长。
2.  **历史记录**: 允许用户保存和查看过去的计时记录。
3.  **自定义显示**: 允许用户选择是否显示百分之一秒，或调整字体大小和颜色。
4.  **振动提醒**: 如果设备支持振动马达，可以在启动/暂停时提供触觉反馈。
5.  **更丰富的UI**: 增加动画效果，例如数字跳动或背景变化。

谢谢大家
