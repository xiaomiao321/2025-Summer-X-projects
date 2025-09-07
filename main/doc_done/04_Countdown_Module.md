# 倒计时模块：精准时间管理与直观交互

## 一、引言

倒计时模块是多功能时钟中一个实用工具，它允许用户设置特定时长，并在时间结束后发出提醒。本模块旨在提供一个直观、易于操作的倒计时功能，支持分钟和秒的精确设置，并提供运行、暂停、恢复等状态管理，以及清晰的视觉反馈。

## 二、实现思路与技术选型

### 2.1 状态管理

倒计时模块的核心在于其复杂的状态管理。它通过几个布尔变量和枚举类型来跟踪倒计时的当前状态：
- `countdown_running`: 倒计时是否正在运行。
- `countdown_paused`: 倒计时是否处于暂停状态。
- `countdown_setting_mode`: 枚举类型，表示当前用户正在设置分钟、秒，还是准备启动倒计时。

这些状态变量确保了用户交互的正确性和逻辑的清晰性。

### 2.2 时间计算与显示

倒计时时间的计算和显示是模块的关键功能。它需要将总毫秒数转换为分钟、秒和百分之一秒，并以大字体居中显示，提供良好的可读性。
- `millis_left`: 剩余毫秒数，是所有时间计算的基础。
- 分钟、秒、百分之一秒的提取：通过除法和取模运算从 `millis_left` 中精确计算。
- 动态布局：根据屏幕宽度和字体大小，动态计算数字和冒号的位置，确保时间始终居中显示。

### 2.3 用户输入处理

模块充分利用了旋转编码器和按键进行交互：
- **旋转编码器**: 在设置模式下，用于增减分钟或秒数。在运行或暂停模式下，旋转编码器无作用。
- **按键**: 
    - 在设置模式下，用于在分钟、秒和启动模式之间切换。
    - 在启动模式下，用于开始倒计时。
    - 在运行模式下，用于暂停倒计时。
    - 在暂停模式下，用于恢复倒计时。
    - 长按按键用于退出倒计时模块。

### 2.4 视觉反馈

为了提升用户体验，模块提供了丰富的视觉反馈：
- **高亮显示**: 在设置模式下，当前正在编辑的分钟或秒数会高亮显示，引导用户操作。
- **状态文本**: 在屏幕下方显示“RUNNING”、“PAUSED”、“FINISHED”或“SET MINUTES/SECONDS/START”等状态文本，清晰指示倒计时状态。
- **进度条**: 屏幕下方有一个进度条，直观地显示倒计时的完成进度。
- **蜂鸣器提醒**: 
    - 在倒计时结束前5秒，每秒发出蜂鸣声作为警告。
    - 倒计时结束时，发出持续的蜂鸣声作为最终提醒。
    - 按键操作时，播放不同的按键音效。

### 2.5 与其他模块的集成

- **时间显示**: 顶部实时时间显示依赖于 `weather.h` 中提供的 `timeinfo` 结构体和 `getLocalTime` 函数。
- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，倒计时模块能够及时退出，避免功能冲突。
- **子菜单退出**: `exitSubMenu` 标志用于从主菜单强制退出当前子菜单。

## 三、代码展示

### `Countdown.cpp`

```c++
#include "Countdown.h"
#include "Menu.h"
#include "MQTT.h"
#include "Buzzer.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include "weather.h"
// --- State Variables ---
static unsigned long countdown_target_millis = 0;
static unsigned long countdown_start_millis = 0;
static bool countdown_running = false;
static bool countdown_paused = false;
static unsigned long countdown_pause_time = 0;
static long countdown_duration_seconds = 5 * 60; // Default to 5 minutes
static unsigned long last_countdown_beep_time = 0; // For 5-second warning

// --- UI Control State ---
enum CountdownSettingMode { MODE_MINUTES, MODE_SECONDS, MODE_READY_TO_START };
static CountdownSettingMode countdown_setting_mode = MODE_MINUTES;

// --- Helper function to draw the countdown UI ---
void displayCountdownTime(unsigned long millis_left) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(TL_DATUM);

    // Display current time at the top
    // timeinfo is a global variable declared in weather.h
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

    

    // --- Time Calculation ---
    unsigned long total_seconds = millis_left / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (millis_left % 1000) / 10;

    // --- Font and Position Calculation ---
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);
    int num_w = menuSprite.textWidth("8");
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    int num_h = menuSprite.fontHeight();
    bool show_hundredths = (countdown_running || countdown_paused);
    int total_width = (num_w * 4) + colon_w + (show_hundredths ? (dot_w + num_w * 2) : 0);
    int current_x = (menuSprite.width() - total_width) / 2;
    int y_pos = (menuSprite.height() / 2) - (num_h / 2) - 20;
    char buf[3];

    // --- Draw Minutes and Seconds with Highlighting ---
    if (!countdown_running && !countdown_paused && countdown_setting_mode == MODE_MINUTES) {
        menuSprite.setTextColor(TFT_BLACK, TFT_YELLOW);
    } else {
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    sprintf(buf, "%02ld", minutes);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK); // Colon is never highlighted
    menuSprite.drawString(":", current_x, y_pos);
    current_x += colon_w;

    if (!countdown_running && !countdown_paused && countdown_setting_mode == MODE_SECONDS) {
        menuSprite.setTextColor(TFT_BLACK, TFT_YELLOW);
    } else {
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    sprintf(buf, "%02ld", seconds);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

    // --- Draw Hundredths (if running) ---
    if (show_hundredths) {
        menuSprite.drawString(".", current_x, y_pos);
        current_x += dot_w;
        sprintf(buf, "%02ld", hundredths);
        menuSprite.drawString(buf, current_x, y_pos);
    }

    // --- Display Status Text ---
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (countdown_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (countdown_paused) {
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (millis_left == 0 && countdown_duration_seconds > 0) {
        menuSprite.drawString("FINISHED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else { // Ready to start or setting time
        if (countdown_setting_mode == MODE_MINUTES) {
            menuSprite.drawString("SET MINUTES", menuSprite.width() / 2, menuSprite.height() - 40);
        } else if (countdown_setting_mode == MODE_SECONDS) {
            menuSprite.drawString("SET SECONDS", menuSprite.width() / 2, menuSprite.height() - 40);
        } else { // MODE_READY_TO_START
            menuSprite.fillRoundRect(80, menuSprite.height() - 60, 80, 40, 5, TFT_GREEN);
            menuSprite.setTextColor(TFT_BLACK);
            menuSprite.drawString("START", menuSprite.width() / 2, menuSprite.height() - 40);
        }
    }

    // --- Draw Progress Bar ---
    float progress = (countdown_duration_seconds > 0) ? 1.0 - (float)millis_left / (countdown_duration_seconds * 1000.0) : 0.0;
    if (progress < 0) progress = 0; if (progress > 1) progress = 1;
    menuSprite.drawRect(20, menuSprite.height() / 2 + 40, menuSprite.width() - 40, 20, TFT_WHITE);
    menuSprite.fillRect(22, menuSprite.height() / 2 + 42, (int)((menuSprite.width() - 44) * progress), 16, TFT_GREEN);

    menuSprite.pushSprite(0, 0);
}

// --- Main Countdown Menu function ---
void CountdownMenu() {
    countdown_running = false;
    countdown_paused = false;
    countdown_duration_seconds = 5 * 60; // Reset to default 5 minutes
    countdown_setting_mode = MODE_MINUTES; // Start in minutes setting mode

    displayCountdownTime(countdown_duration_seconds * 1000);

    unsigned long last_display_update_time = millis();

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false;
            return;
        }
        if (g_alarm_is_ringing) { return; } 
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100);
            return;
        }

        int encoder_value = readEncoder();
        bool button_pressed = readButton();

        // --- Logic when timer is NOT running ---
        if (!countdown_running && !countdown_paused) {
            if (encoder_value != 0) {
                if (countdown_setting_mode == MODE_MINUTES) {
                    countdown_duration_seconds += encoder_value * 60;
                } else if (countdown_setting_mode == MODE_SECONDS) {
                    long current_minutes = countdown_duration_seconds / 60;
                    long current_seconds = countdown_duration_seconds % 60;
                    current_seconds += encoder_value;
                    if (current_seconds >= 60) { current_seconds = 0; current_minutes++; }
                    else if (current_seconds < 0) { current_seconds = 59; if(current_minutes > 0) current_minutes--; }
                    countdown_duration_seconds = (current_minutes * 60) + current_seconds;
                }
                if (countdown_duration_seconds < 0) countdown_duration_seconds = 0;
                displayCountdownTime(countdown_duration_seconds * 1000);
                tone(BUZZER_PIN, 1000, 20);
            }

            if (button_pressed) {
                tone(BUZZER_PIN, 2000, 50);
                if (countdown_setting_mode == MODE_READY_TO_START) {
                    // START THE TIMER
                    if (countdown_duration_seconds > 0) {
                        countdown_start_millis = millis();
                        countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                        countdown_running = true;
                        countdown_paused = false;
                        last_countdown_beep_time = 0; // Reset beep timer
                    } else { // If time is 0, just reset mode
                        countdown_setting_mode = MODE_MINUTES;
                    }
                } else {
                    // Cycle through setting modes
                    countdown_setting_mode = (CountdownSettingMode)((countdown_setting_mode + 1) % 3);
                }
                displayCountdownTime(countdown_duration_seconds * 1000);
            }
        } 
        // --- Logic when timer IS running or paused ---
        else {
            if (button_pressed) {
                tone(BUZZER_PIN, 2000, 50);
                if (countdown_running) { // PAUSE
                    countdown_pause_time = millis();
                    countdown_running = false;
                    countdown_paused = true;
                } else { // RESUME
                    countdown_start_millis += (millis() - countdown_pause_time);
                    countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                    countdown_running = true;
                    countdown_paused = false;
                }
            }
        }

        // --- Update display if running ---
        if (countdown_running) {
            unsigned long current_millis = millis();
            long millis_left = countdown_target_millis - current_millis;
            if (millis_left < 0) millis_left = 0;

            // Update countdown display frequently
            if (millis_left / 100 != (countdown_target_millis - last_display_update_time) / 100) {
                displayCountdownTime(millis_left);
                last_display_update_time = current_millis;
            }

            // 5-second warning
            if (millis_left > 0 && millis_left <= 5000 && (current_millis - last_countdown_beep_time >= 1000 || last_countdown_beep_time == 0)) {
                tone(BUZZER_PIN, 1000, 100);
                last_countdown_beep_time = current_millis;
            }

            if (millis_left == 0) {
                countdown_running = false;
                countdown_paused = false;
                tone(BUZZER_PIN, 3000, 3000); // Final alarm sound
                displayCountdownTime(0);
            }
        } else { // If countdown is not running (paused or setting time), update real-time clock
            unsigned long current_millis = millis();
            if (current_millis - last_display_update_time >= 1000) { // Update every second
                displayCountdownTime(countdown_duration_seconds * 1000); // Pass current countdown value (won't change)
                last_display_update_time = current_millis;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## 四、代码解读

### 4.1 状态变量与UI控制

```c++
// --- State Variables ---
static unsigned long countdown_target_millis = 0;
static unsigned long countdown_start_millis = 0;
static bool countdown_running = false;
static bool countdown_paused = false;
static unsigned long countdown_pause_time = 0;
static long countdown_duration_seconds = 5 * 60; // Default to 5 minutes
static unsigned long last_countdown_beep_time = 0; // For 5-second warning

// --- UI Control State ---
enum CountdownSettingMode { MODE_MINUTES, MODE_SECONDS, MODE_READY_TO_START };
static CountdownSettingMode countdown_setting_mode = MODE_MINUTES;
```
- `countdown_target_millis`: 倒计时结束时的 `millis()` 值。
- `countdown_start_millis`: 倒计时开始时的 `millis()` 值。
- `countdown_running`: 倒计时是否正在运行。
- `countdown_paused`: 倒计时是否处于暂停状态。
- `countdown_pause_time`: 记录暂停时的 `millis()` 值，用于计算恢复时需要补偿的时间。
- `countdown_duration_seconds`: 用户设置的倒计时总时长（秒）。
- `last_countdown_beep_time`: 用于控制倒计时结束前5秒的蜂鸣器警告频率。
- `CountdownSettingMode`: 枚举类型，定义了三种设置模式：设置分钟、设置秒、准备启动。

### 4.2 倒计时UI绘制 `displayCountdownTime()`

```c++
void displayCountdownTime(unsigned long millis_left) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(TL_DATUM);

    // Display current time at the top
    // ... (实时时间显示逻辑)

    // --- Time Calculation ---
    unsigned long total_seconds = millis_left / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (millis_left % 1000) / 10;

    // --- Font and Position Calculation ---
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);
    int num_w = menuSprite.textWidth("8");
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    int num_h = menuSprite.fontHeight();
    bool show_hundredths = (countdown_running || countdown_paused);
    int total_width = (num_w * 4) + colon_w + (show_hundredths ? (dot_w + num_w * 2) : 0);
    int current_x = (menuSprite.width() - total_width) / 2;
    int y_pos = (menuSprite.height() / 2) - (num_h / 2) - 20;
    char buf[3];

    // --- Draw Minutes and Seconds with Highlighting ---
    if (!countdown_running && !countdown_paused && countdown_setting_mode == MODE_MINUTES) {
        menuSprite.setTextColor(TFT_BLACK, TFT_YELLOW);
    } else {
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    sprintf(buf, "%02ld", minutes);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK); // Colon is never highlighted
    menuSprite.drawString(":", current_x, y_pos);
    current_x += colon_w;

    if (!countdown_running && !countdown_paused && countdown_setting_mode == MODE_SECONDS) {
        menuSprite.setTextColor(TFT_BLACK, TFT_YELLOW);
    } else {
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    sprintf(buf, "%02ld", seconds);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

    // --- Draw Hundredths (if running) ---
    if (show_hundredths) {
        menuSprite.drawString(".", current_x, y_pos);
        current_x += dot_w;
        sprintf(buf, "%02ld", hundredths);
        menuSprite.drawString(buf, current_x, y_pos);
    }

    // --- Display Status Text ---
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (countdown_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (countdown_paused) {
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (millis_left == 0 && countdown_duration_seconds > 0) {
        menuSprite.drawString("FINISHED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else { // Ready to start or setting time
        if (countdown_setting_mode == MODE_MINUTES) {
            menuSprite.drawString("SET MINUTES", menuSprite.width() / 2, menuSprite.height() - 40);
        } else if (countdown_setting_mode == MODE_SECONDS) {
            menuSprite.drawString("SET SECONDS", menuSprite.width() / 2, menuSprite.height() - 40);
        } else { // MODE_READY_TO_START
            menuSprite.fillRoundRect(80, menuSprite.height() - 60, 80, 40, 5, TFT_GREEN);
            menuSprite.setTextColor(TFT_BLACK);
            menuSprite.drawString("START", menuSprite.width() / 2, menuSprite.height() - 40);
        }
    }

    // --- Draw Progress Bar ---
    float progress = (countdown_duration_seconds > 0) ? 1.0 - (float)millis_left / (countdown_duration_seconds * 1000.0) : 0.0;
    if (progress < 0) progress = 0; if (progress > 1) progress = 1;
    menuSprite.drawRect(20, menuSprite.height() / 2 + 40, menuSprite.width() - 40, 20, TFT_WHITE);
    menuSprite.fillRect(22, menuSprite.height() / 2 + 42, (int)((menuSprite.width() - 44) * progress), 16, TFT_GREEN);

    menuSprite.pushSprite(0, 0);
}
```
- **实时时间显示**: 顶部显示当前系统时间，与 `weather.cpp` 模块共享 `timeinfo`。
- **时间计算**: 将传入的 `millis_left` 转换为分钟、秒和百分之一秒，用于显示。
- **动态布局**: 计算数字和符号的宽度，以确保整个时间字符串在屏幕上居中显示。
- **高亮显示**: 根据 `countdown_setting_mode` 的值，高亮显示当前正在设置的分钟或秒，通过改变文本颜色和背景色实现。
- **状态文本**: 根据 `countdown_running`、`countdown_paused` 和 `millis_left` 的值，显示不同的状态提示（如“RUNNING”、“PAUSED”、“FINISHED”或设置提示）。
- **启动按钮**: 在 `MODE_READY_TO_START` 模式下，绘制一个绿色的“START”按钮。
- **进度条**: 绘制一个矩形进度条，其填充宽度根据倒计时进度动态变化，直观展示剩余时间。
- **双缓冲**: 所有绘制操作都在 `menuSprite` 上进行，最后通过 `menuSprite.pushSprite(0, 0)` 推送到屏幕，避免闪烁。

### 4.3 主倒计时函数 `CountdownMenu()`

```c++
void CountdownMenu() {
    countdown_running = false;
    countdown_paused = false;
    countdown_duration_seconds = 5 * 60; // Reset to default 5 minutes
    countdown_setting_mode = MODE_MINUTES; // Start in minutes setting mode

    displayCountdownTime(countdown_duration_seconds * 1000);

    unsigned long last_display_update_time = millis();

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false;
            return;
        }
        if (g_alarm_is_ringing) { return; } 
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100);
            return;
        }

        int encoder_value = readEncoder();
        bool button_pressed = readButton();

        // --- Logic when timer is NOT running ---
        if (!countdown_running && !countdown_paused) {
            if (encoder_value != 0) {
                if (countdown_setting_mode == MODE_MINUTES) {
                    countdown_duration_seconds += encoder_value * 60;
                } else if (countdown_setting_mode == MODE_SECONDS) {
                    long current_minutes = countdown_duration_seconds / 60;
                    long current_seconds = countdown_duration_seconds % 60;
                    current_seconds += encoder_value;
                    if (current_seconds >= 60) { current_seconds = 0; current_minutes++; }
                    else if (current_seconds < 0) { current_seconds = 59; if(current_minutes > 0) current_minutes--; }
                    countdown_duration_seconds = (current_minutes * 60) + current_seconds;
                }
                if (countdown_duration_seconds < 0) countdown_duration_seconds = 0;
                displayCountdownTime(countdown_duration_seconds * 1000);
                tone(BUZZER_PIN, 1000, 20);
            }

            if (button_pressed) {
                tone(BUZZER_PIN, 2000, 50);
                if (countdown_setting_mode == MODE_READY_TO_START) {
                    // START THE TIMER
                    if (countdown_duration_seconds > 0) {
                        countdown_start_millis = millis();
                        countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                        countdown_running = true;
                        countdown_paused = false;
                        last_countdown_beep_time = 0; // Reset beep timer
                    } else { // If time is 0, just reset mode
                        countdown_setting_mode = MODE_MINUTES;
                    }
                } else {
                    // Cycle through setting modes
                    countdown_setting_mode = (CountdownSettingMode)((countdown_setting_mode + 1) % 3);
                }
                displayCountdownTime(countdown_duration_seconds * 1000);
            }
        } 
        // --- Logic when timer IS running or paused ---
        else {
            if (button_pressed) {
                tone(BUZZER_PIN, 2000, 50);
                if (countdown_running) { // PAUSE
                    countdown_pause_time = millis();
                    countdown_running = false;
                    countdown_paused = true;
                } else { // RESUME
                    countdown_start_millis += (millis() - countdown_pause_time);
                    countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                    countdown_running = true;
                    countdown_paused = false;
                }
            }
        }

        // --- Update display if running ---
        if (countdown_running) {
            unsigned long current_millis = millis();
            long millis_left = countdown_target_millis - current_millis;
            if (millis_left < 0) millis_left = 0;

            // Update countdown display frequently
            if (millis_left / 100 != (countdown_target_millis - last_display_update_time) / 100) {
                displayCountdownTime(millis_left);
                last_display_update_time = current_millis;
            }

            // 5-second warning
            if (millis_left > 0 && millis_left <= 5000 && (current_millis - last_countdown_beep_time >= 1000 || last_countdown_beep_time == 0)) {
                tone(BUZZER_PIN, 1000, 100);
                last_countdown_beep_time = current_millis;
            }

            if (millis_left == 0) {
                countdown_running = false;
                countdown_paused = false;
                tone(BUZZER_PIN, 3000, 3000); // Final alarm sound
                displayCountdownTime(0);
            }
        } else { // If countdown is not running (paused or setting time), update real-time clock
            unsigned long current_millis = millis();
            if (current_millis - last_display_update_time >= 1000) { // Update every second
                displayCountdownTime(countdown_duration_seconds * 1000); // Pass current countdown value (won't change)
                last_display_update_time = current_millis;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```
- **初始化**: 进入 `CountdownMenu` 时，重置所有状态变量为初始值（倒计时停止，默认5分钟，设置模式为分钟）。
- **主循环**: `while(true)` 循环持续运行，直到满足退出条件。
- **退出条件**: 
    - `exitSubMenu`: 由主菜单触发的退出标志。
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `readButtonLongPress()`: 用户长按按键退出。
- **设置模式逻辑**: 
    - 当倒计时未运行也未暂停时，旋转编码器用于调整 `countdown_duration_seconds`（分钟或秒）。
    - 单击按键用于在 `MODE_MINUTES`、`MODE_SECONDS` 和 `MODE_READY_TO_START` 之间切换。
    - 在 `MODE_READY_TO_START` 模式下单击按键，如果设置时长大于0，则启动倒计时。
- **运行/暂停逻辑**: 
    - 当倒计时正在运行时，单击按键会暂停倒计时，并记录 `countdown_pause_time`。
    - 当倒计时暂停时，单击按键会恢复倒计时，并根据暂停时长调整 `countdown_start_millis`。
- **显示更新**: 
    - 倒计时运行时，每100毫秒更新一次显示，确保百分之一秒的流畅显示。
    - 倒计时未运行时（暂停或设置），每秒更新一次显示，以刷新顶部的实时时间。
- **蜂鸣器警告**: 在倒计时结束前5秒，每秒触发一次蜂鸣器短音。
- **倒计时结束**: 当 `millis_left` 变为0时，倒计时停止，播放长时间的蜂鸣器音，并将状态重置。
- **`vTaskDelay`**: 引入短暂延时，避免忙等待，降低CPU负载。

## 五、总结与展望

倒计时模块提供了一个功能完善、交互友好的倒计时工具。其精细的状态管理、直观的视觉反馈和与硬件的紧密结合，使其成为多功能时钟中不可或缺的一部分。

未来的改进方向：
1.  **预设时长**: 允许用户保存和快速选择常用的倒计时时长（例如，1分钟、5分钟、30分钟）。
2.  **多任务倒计时**: 支持同时运行多个独立的倒计时，并在屏幕上切换显示或概览。
3.  **自定义结束音效**: 允许用户选择不同的蜂鸣器音效或播放自定义音乐作为倒计时结束提醒。
4.  **振动提醒**: 如果设备支持振动马达，可以增加振动提醒功能。
5.  **更丰富的UI**: 增加动画效果，例如倒计时结束时的闪烁效果。

谢谢大家
