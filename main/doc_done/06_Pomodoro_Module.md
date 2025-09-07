# 番茄钟模块：专注与休息的平衡

## 一、引言

番茄工作法是一种流行的效率管理技术，它将工作时间划分为25分钟的专注工作时段（番茄），然后是短暂的休息，每完成几个番茄后进行一次长时间的休息。番茄钟模块旨在为用户提供一个易于使用的番茄工作法计时器，帮助用户保持专注，合理安排工作与休息。

## 二、实现思路与技术选型

### 2.1 状态机管理

番茄钟模块的核心是一个清晰的状态机，它定义了番茄钟可能处于的各种状态以及状态之间的转换逻辑：
- `STATE_IDLE`: 空闲状态，等待用户开始。
- `STATE_WORK`: 工作时段。
- `STATE_SHORT_BREAK`: 短暂休息时段。
- `STATE_LONG_BREAK`: 长时间休息时段。
- `STATE_PAUSED`: 暂停状态，可以从工作或休息状态进入。

通过状态机，模块能够准确地跟踪当前所处的阶段，并根据用户交互或时间流逝进行状态切换。

### 2.2 可配置的时长

工作时段、短休息和长休息的时长都是可配置的常量，方便用户根据自己的需求进行调整。模块还跟踪已完成的工作时段数量，以便在达到预设数量后自动进入长休息。

### 2.3 用户界面与视觉反馈

模块提供了直观的UI，帮助用户了解番茄钟的当前状态和进度：
- **状态显示**: 屏幕中央显示当前状态（如“Work”、“Short Break”等）。
- **圆形进度条**: 一个动态的圆形进度条直观地展示当前时段的剩余时间，颜色会根据工作/休息状态而变化。
- **时间显示**: 进度条中央以大字体显示剩余时间（分钟和秒）。
- **会话标记**: 屏幕下方显示已完成的工作时段数量，通过小圆点标记，帮助用户跟踪进度。

### 2.4 用户交互

模块主要通过一个按键进行交互：
- **单击**: 
    - 在 `STATE_IDLE` 状态下，开始一个工作时段。
    - 在工作或休息状态下，暂停计时。
    - 在 `STATE_PAUSED` 状态下，恢复计时。
- **长按**: 退出番茄钟模块，并重置所有状态。

### 2.5 声音反馈

- **5秒警告**: 在每个时段结束前5秒，每秒发出蜂鸣声作为提醒。
- **时段结束提醒**: 每个时段结束时，发出一个持续的蜂鸣声作为完成提醒。

### 2.6 与其他模块的集成

- **实时时间显示**: 顶部实时时间显示依赖于 `weather.h` 中提供的 `timeinfo` 结构体和 `getLocalTime` 函数。
- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，番茄钟模块能够及时退出，避免功能冲突。

## 三、代码展示

### `Pomodoro.cpp`

```c++
#include "Pomodoro.h"
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Buzzer.h"
#include "animation.h" // For smoothArc
#include "Alarm.h"
#include "weather.h"
// --- Configuration ---
const unsigned long WORK_DURATION_SECS = 25 * 60;
const unsigned long SHORT_BREAK_DURATION_SECS = 5 * 60;
const unsigned long LONG_BREAK_DURATION_SECS = 15 * 60;
const int SESSIONS_BEFORE_LONG_BREAK = 4;

// --- State Machine ---
enum PomodoroState { 
    STATE_IDLE, 
    STATE_WORK, 
    STATE_SHORT_BREAK, 
    STATE_LONG_BREAK, 
    STATE_PAUSED 
};

// --- Module-static State Variables ---
static PomodoroState currentState = STATE_IDLE;
static PomodoroState stateBeforePause = STATE_IDLE;
static unsigned long session_end_millis = 0;
static unsigned long remaining_on_pause = 0;
static int sessions_completed = 0;
static unsigned long last_pomodoro_beep_time = 0; // For 5-second warning

// =====================================================================================
//                                     DRAWING LOGIC
// =====================================================================================

static void drawPomodoroUI(unsigned long remaining_secs, unsigned long total_secs) {
    menuSprite.fillScreen(TFT_BLACK);

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

    menuSprite.setTextDatum(MC_DATUM);

    // --- Draw Title / Current State ---
    menuSprite.setTextFont(1);
    menuSprite.setTextColor(TFT_WHITE);
    const char* state_text = "";
    uint16_t arc_color = TFT_DARKGREY;
    switch(currentState) {
        case STATE_IDLE: state_text = "Ready"; arc_color = TFT_SKYBLUE; break;
        case STATE_WORK: state_text = "Work"; arc_color = TFT_ORANGE; break;
        case STATE_SHORT_BREAK: state_text = "Short Break"; arc_color = TFT_GREEN; break;
        case STATE_LONG_BREAK: state_text = "Long Break"; arc_color = TFT_CYAN; break;
        case STATE_PAUSED: state_text = "Paused"; arc_color = TFT_YELLOW; break;
    }
    menuSprite.setTextSize(2);
    menuSprite.drawString(state_text, 120,180 );
    menuSprite.setTextSize(1);

    // --- Draw Circular Progress Bar (Radius 55) ---
    float progress = (total_secs > 0) ? (float)remaining_secs / total_secs : 0;
    int angle = 360 * progress;
    // Arc center (x,y) = (120, 115)
    // Radius = 55
    // Thickness = 10
    menuSprite.drawSmoothArc(120, 120, 100, 80, 0, 360, TFT_DARKGREY, TFT_BLACK, true); // Background arc
    menuSprite.drawSmoothArc(120, 120, 100-5, 80+5, 0, angle, arc_color, TFT_BLACK, true);    // Foreground arc

    // --- Draw Time (Font 7, Size 1 - consistent with Countdown.cpp) ---
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_WHITE);
    char time_buf[6];
    sprintf(time_buf, "%02lu:%02lu", remaining_secs / 60, remaining_secs % 60);
    menuSprite.drawString(time_buf, 120, 115);

    // --- Draw Session Markers ---
    int marker_y = 230;
    int marker_total_width = SESSIONS_BEFORE_LONG_BREAK * 20;
    int marker_start_x = 120 - (marker_total_width / 2);
    for (int i = 0; i < SESSIONS_BEFORE_LONG_BREAK; i++) {
        if (i < sessions_completed) {
            menuSprite.fillCircle(marker_start_x + (i * 20), marker_y, 6, TFT_GREEN);
        } else {
            menuSprite.drawCircle(marker_start_x + (i * 20), marker_y, 6, TFT_DARKGREY);
        }
    }

    menuSprite.pushSprite(0, 0);
}

// =====================================================================================
//                                     CORE LOGIC
// =====================================================================================

static void startNewSession(PomodoroState nextState) {
    currentState = nextState;
    unsigned long duration_secs = 0;

    switch(currentState) {
        case STATE_WORK: duration_secs = WORK_DURATION_SECS; break;
        case STATE_SHORT_BREAK: duration_secs = SHORT_BREAK_DURATION_SECS; break;
        case STATE_LONG_BREAK: 
            duration_secs = LONG_BREAK_DURATION_SECS; 
            sessions_completed = 0;
            break;
        default: break;
    }

    session_end_millis = millis() + (duration_secs * 1000);
    drawPomodoroUI(duration_secs, duration_secs);
    last_pomodoro_beep_time = 0; // Reset beep timer
}

void PomodoroMenu() {
    currentState = STATE_IDLE;
    sessions_completed = 0;
    drawPomodoroUI(WORK_DURATION_SECS, WORK_DURATION_SECS); // Initial draw for IDLE state
    unsigned long last_draw_time = 0; // For controlling redraw frequency
    unsigned long last_realtime_clock_update = millis(); // For real-time clock update

    while(true) {
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100);
            menuSprite.setTextFont(1); // Reset font on exit
            menuSprite.setTextSize(1);
            return; // Reset and Exit
        }

        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50);
            if (currentState == STATE_IDLE) {
                startNewSession(STATE_WORK);
            } else if (currentState == STATE_PAUSED) {
                currentState = stateBeforePause;
                session_end_millis = millis() + remaining_on_pause;
            } else { // Any running state
                remaining_on_pause = session_end_millis - millis();
                stateBeforePause = currentState;
                currentState = STATE_PAUSED;
                // Recalculate total_secs for paused state display
                unsigned long total_secs_on_pause = 0;
                 switch(stateBeforePause) {
                    case STATE_WORK: total_secs_on_pause = WORK_DURATION_SECS; break;
                    case STATE_SHORT_BREAK: total_secs_on_pause = SHORT_BREAK_DURATION_SECS; break;
                    case STATE_LONG_BREAK: total_secs_on_pause = LONG_BREAK_DURATION_SECS; break;
                    default: break;
                }
                drawPomodoroUI(remaining_on_pause / 1000, total_secs_on_pause);
            }
        }

        // --- Timer tick logic ---
        if (currentState != STATE_IDLE && currentState != STATE_PAUSED) {
            unsigned long time_now = millis();
            unsigned long remaining_ms = (time_now >= session_end_millis) ? 0 : session_end_millis - time_now;
            
            // Redraw every second to keep the timer ticking
            if (time_now - last_draw_time >= 990) { // Use >= 990 to ensure redraws happen once per second
                unsigned long remaining_secs = remaining_ms / 1000;
                unsigned long total_secs = 0;
                switch(currentState) {
                    case STATE_WORK: total_secs = WORK_DURATION_SECS; break;
                    case STATE_SHORT_BREAK: total_secs = SHORT_BREAK_DURATION_SECS; break;
                    case STATE_LONG_BREAK: total_secs = LONG_BREAK_DURATION_SECS; break;
                    default: break;
                }
                drawPomodoroUI(remaining_secs, total_secs);
                last_draw_time = time_now;
            }

            // 5-second warning
            if (remaining_ms > 0 && remaining_ms <= 5000 && (time_now - last_pomodoro_beep_time >= 1000 || last_pomodoro_beep_time == 0)) {
                tone(BUZZER_PIN, 1000, 100);
                last_pomodoro_beep_time = time_now;
            }

            if (remaining_ms == 0) {
                tone(BUZZER_PIN, 3000, 3000); // Final alarm sound
                if (currentState == STATE_WORK) {
                    sessions_completed++;
                    if (sessions_completed >= SESSIONS_BEFORE_LONG_BREAK) {
                        startNewSession(STATE_LONG_BREAK);
                    } else {
                        startNewSession(STATE_SHORT_BREAK);
                    }
                } else { // A break finished
                    startNewSession(STATE_WORK);
                }
            }
        } else { // If not running (IDLE or PAUSED), update real-time clock
            unsigned long current_millis = millis();
            if (current_millis - last_realtime_clock_update >= 1000) { // Update every second
                unsigned long current_remaining_secs = 0;
                unsigned long current_total_secs = 0;

                if (currentState == STATE_IDLE) {
                    current_remaining_secs = WORK_DURATION_SECS;
                    current_total_secs = WORK_DURATION_SECS;
                } else if (currentState == STATE_PAUSED) {
                    current_remaining_secs = remaining_on_pause / 1000;
                    switch(stateBeforePause) {
                        case STATE_WORK: current_total_secs = WORK_DURATION_SECS; break;
                        case STATE_SHORT_BREAK: current_total_secs = SHORT_BREAK_DURATION_SECS; break;
                        case STATE_LONG_BREAK: current_total_secs = LONG_BREAK_DURATION_SECS; break;
                        default: current_total_secs = WORK_DURATION_SECS; break; // Fallback
                    }
                }
                drawPomodoroUI(current_remaining_secs, current_total_secs);
                last_realtime_clock_update = current_millis;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

## 四、代码解读

### 4.1 配置与状态变量

```c++
// --- Configuration ---
const unsigned long WORK_DURATION_SECS = 25 * 60;
const unsigned long SHORT_BREAK_DURATION_SECS = 5 * 60;
const unsigned long LONG_BREAK_DURATION_SECS = 15 * 60;
const int SESSIONS_BEFORE_LONG_BREAK = 4;

// --- State Machine ---
enum PomodoroState { 
    STATE_IDLE, 
    STATE_WORK, 
    STATE_SHORT_BREAK, 
    STATE_LONG_BREAK, 
    STATE_PAUSED 
};

// --- Module-static State Variables ---
static PomodoroState currentState = STATE_IDLE;
static PomodoroState stateBeforePause = STATE_IDLE;
static unsigned long session_end_millis = 0;
static unsigned long remaining_on_pause = 0;
static int sessions_completed = 0;
static unsigned long last_pomodoro_beep_time = 0; // For 5-second warning
```
- `WORK_DURATION_SECS`, `SHORT_BREAK_DURATION_SECS`, `LONG_BREAK_DURATION_SECS`: 定义了工作和休息时段的默认时长（秒）。
- `SESSIONS_BEFORE_LONG_BREAK`: 定义了多少个工作时段后进入长休息。
- `PomodoroState`: 枚举类型，定义了番茄钟的五种状态。
- `currentState`: 当前番茄钟的状态。
- `stateBeforePause`: 记录进入暂停状态前的状态，以便恢复。
- `session_end_millis`: 当前时段结束时的 `millis()` 时间戳。
- `remaining_on_pause`: 暂停时剩余的时间（毫秒）。
- `sessions_completed`: 已完成的工作时段数量。
- `last_pomodoro_beep_time`: 用于控制5秒警告音的频率。

### 4.2 UI 绘制 `drawPomodoroUI()`

```c++
static void drawPomodoroUI(unsigned long remaining_secs, unsigned long total_secs) {
    menuSprite.fillScreen(TFT_BLACK);

    // Display current time at the top
    // ... (实时时间显示逻辑)

    menuSprite.setTextDatum(MC_DATUM);

    // --- Draw Title / Current State ---
    menuSprite.setTextFont(1);
    menuSprite.setTextColor(TFT_WHITE);
    const char* state_text = "";
    uint16_t arc_color = TFT_DARKGREY;
    switch(currentState) {
        case STATE_IDLE: state_text = "Ready"; arc_color = TFT_SKYBLUE; break;
        case STATE_WORK: state_text = "Work"; arc_color = TFT_ORANGE; break;
        case STATE_SHORT_BREAK: state_text = "Short Break"; arc_color = TFT_GREEN; break;
        case STATE_LONG_BREAK: state_text = "Long Break"; arc_color = TFT_CYAN; break;
        case STATE_PAUSED: state_text = "Paused"; arc_color = TFT_YELLOW; break;
    }
    menuSprite.setTextSize(2);
    menuSprite.drawString(state_text, 120,180 );
    menuSprite.setTextSize(1);

    // --- Draw Circular Progress Bar (Radius 55) ---
    float progress = (total_secs > 0) ? (float)remaining_secs / total_secs : 0;
    int angle = 360 * progress;
    // Arc center (x,y) = (120, 115)
    // Radius = 55
    // Thickness = 10
    menuSprite.drawSmoothArc(120, 120, 100, 80, 0, 360, TFT_DARKGREY, TFT_BLACK, true); // Background arc
    menuSprite.drawSmoothArc(120, 120, 100-5, 80+5, 0, angle, arc_color, TFT_BLACK, true);    // Foreground arc

    // --- Draw Time (Font 7, Size 1 - consistent with Countdown.cpp) ---
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_WHITE);
    char time_buf[6];
    sprintf(time_buf, "%02lu:%02lu", remaining_secs / 60, remaining_secs % 60);
    menuSprite.drawString(time_buf, 120, 115);

    // --- Draw Session Markers ---
    int marker_y = 230;
    int marker_total_width = SESSIONS_BEFORE_LONG_BREAK * 20;
    int marker_start_x = 120 - (marker_total_width / 2);
    for (int i = 0; i < SESSIONS_BEFORE_LONG_BREAK; i++) {
        if (i < sessions_completed) {
            menuSprite.fillCircle(marker_start_x + (i * 20), marker_y, 6, TFT_GREEN);
        } else {
            menuSprite.drawCircle(marker_start_x + (i * 20), marker_y, 6, TFT_DARKGREY);
        }
    }

    menuSprite.pushSprite(0, 0);
}
```
- **实时时间显示**: 顶部显示当前系统时间。
- **状态文本**: 根据 `currentState` 显示不同的状态文本，并根据状态选择圆形进度条的颜色。
- **圆形进度条**: 使用 `drawSmoothArc` 函数绘制圆形进度条。背景弧线为深灰色，前景弧线颜色根据当前状态变化，其角度根据 `remaining_secs` 和 `total_secs` 计算的进度动态变化。
- **时间显示**: 在圆形进度条中央显示剩余时间（分钟和秒）。
- **会话标记**: 屏幕下方绘制一系列小圆点，已完成的会话显示为绿色实心圆，未完成的显示为灰色空心圆。

### 4.3 核心逻辑 `startNewSession()`

```c++
static void startNewSession(PomodoroState nextState) {
    currentState = nextState;
    unsigned long duration_secs = 0;

    switch(currentState) {
        case STATE_WORK: duration_secs = WORK_DURATION_SECS; break;
        case STATE_SHORT_BREAK: duration_secs = SHORT_BREAK_DURATION_SECS; break;
        case STATE_LONG_BREAK: 
            duration_secs = LONG_BREAK_DURATION_SECS; 
            sessions_completed = 0;
            break;
        default: break;
    }

    session_end_millis = millis() + (duration_secs * 1000);
    drawPomodoroUI(duration_secs, duration_secs);
    last_pomodoro_beep_time = 0; // Reset beep timer
}
```
- 此函数用于启动一个新的番茄钟时段（工作、短休息或长休息）。
- 根据 `nextState` 设置 `currentState` 并确定当前时段的总时长 `duration_secs`。
- 计算 `session_end_millis`，即当前时段结束的毫秒时间戳。
- 如果进入长休息，重置 `sessions_completed` 计数。
- 调用 `drawPomodoroUI` 绘制初始界面。
- 重置 `last_pomodoro_beep_time` 以确保5秒警告音能正常触发。

### 4.4 主番茄钟函数 `PomodoroMenu()`

```c++
void PomodoroMenu() {
    currentState = STATE_IDLE;
    sessions_completed = 0;
    drawPomodoroUI(WORK_DURATION_SECS, WORK_DURATION_SECS); // Initial draw for IDLE state
    unsigned long last_draw_time = 0; // For controlling redraw frequency
    unsigned long last_realtime_clock_update = millis(); // For real-time clock update

    while(true) {
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100);
            menuSprite.setTextFont(1); // Reset font on exit
            menuSprite.setTextSize(1);
            return; // Reset and Exit
        }

        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50);
            if (currentState == STATE_IDLE) {
                startNewSession(STATE_WORK);
            } else if (currentState == STATE_PAUSED) {
                currentState = stateBeforePause;
                session_end_millis = millis() + remaining_on_pause;
            } else { // Any running state
                remaining_on_pause = session_end_millis - millis();
                stateBeforePause = currentState;
                currentState = STATE_PAUSED;
                // Recalculate total_secs for paused state display
                unsigned long total_secs_on_pause = 0;
                 switch(stateBeforePause) {
                    case STATE_WORK: total_secs_on_pause = WORK_DURATION_SECS; break;
                    case STATE_SHORT_BREAK: total_secs_on_pause = SHORT_BREAK_DURATION_SECS; break;
                    case STATE_LONG_BREAK: total_secs_on_pause = LONG_BREAK_DURATION_SECS; break;
                    default: break;
                }
                drawPomodoroUI(remaining_on_pause / 1000, total_secs_on_pause);
            }
        }

        // --- Timer tick logic ---
        if (currentState != STATE_IDLE && currentState != STATE_PAUSED) {
            unsigned long time_now = millis();
            unsigned long remaining_ms = (time_now >= session_end_millis) ? 0 : session_end_millis - time_now;
            
            // Redraw every second to keep the timer ticking
            if (time_now - last_draw_time >= 990) { // Use >= 990 to ensure redraws happen once per second
                unsigned long remaining_secs = remaining_ms / 1000;
                unsigned long total_secs = 0;
                switch(currentState) {
                    case STATE_WORK: total_secs = WORK_DURATION_SECS; break;
                    case STATE_SHORT_BREAK: total_secs = SHORT_BREAK_DURATION_SECS; break;
                    case STATE_LONG_BREAK: total_secs = LONG_BREAK_DURATION_SECS; break;
                    default: break;
                }
                drawPomodoroUI(remaining_secs, total_secs);
                last_draw_time = time_now;
            }

            // 5-second warning
            if (remaining_ms > 0 && remaining_ms <= 5000 && (time_now - last_pomodoro_beep_time >= 1000 || last_pomodoro_beep_time == 0)) {
                tone(BUZZER_PIN, 1000, 100);
                last_pomodoro_beep_time = time_now;
            }

            if (remaining_ms == 0) {
                tone(BUZZER_PIN, 3000, 3000); // Final alarm sound
                if (currentState == STATE_WORK) {
                    sessions_completed++;
                    if (sessions_completed >= SESSIONS_BEFORE_LONG_BREAK) {
                        startNewSession(STATE_LONG_BREAK);
                    } else {
                        startNewSession(STATE_SHORT_BREAK);
                    }
                } else { // A break finished
                    startNewSession(STATE_WORK);
                }
            }
        } else { // If not running (IDLE or PAUSED), update real-time clock
            unsigned long current_millis = millis();
            if (current_millis - last_realtime_clock_update >= 1000) { // Update every second
                unsigned long current_remaining_secs = 0;
                unsigned long current_total_secs = 0;

                if (currentState == STATE_IDLE) {
                    current_remaining_secs = WORK_DURATION_SECS;
                    current_total_secs = WORK_DURATION_SECS;
                } else if (currentState == STATE_PAUSED) {
                    current_remaining_secs = remaining_on_pause / 1000;
                    switch(stateBeforePause) {
                        case STATE_WORK: current_total_secs = WORK_DURATION_SECS; break;
                        case STATE_SHORT_BREAK: current_total_secs = SHORT_BREAK_DURATION_SECS; break;
                        case STATE_LONG_BREAK: current_total_secs = LONG_BREAK_DURATION_SECS; break;
                        default: current_total_secs = WORK_DURATION_SECS; break; // Fallback
                    }
                }
                drawPomodoroUI(current_remaining_secs, current_total_secs);
                last_realtime_clock_update = current_millis;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```
- **初始化**: 进入 `PomodoroMenu` 时，将状态重置为 `STATE_IDLE`，已完成会话数清零，并绘制初始UI。
- **主循环**: `while(true)` 循环持续运行，直到满足退出条件。
- **退出条件**: 
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `readButtonLongPress()`: 用户长按按键退出。
- **按键交互**: 
    - 单击 `readButton()`: 
        - 如果当前是 `STATE_IDLE`，则开始一个工作时段。
        - 如果当前是 `STATE_PAUSED`，则恢复到暂停前的状态。
        - 如果当前是 `STATE_WORK`、`STATE_SHORT_BREAK` 或 `STATE_LONG_BREAK`，则暂停当前时段。
- **计时逻辑**: 
    - 当番茄钟处于工作或休息状态时，每秒更新一次UI，显示剩余时间。
    - 在时段结束前5秒，每秒播放警告音。
    - 当时段结束时 (`remaining_ms == 0`)，播放结束音，并根据当前状态决定下一个状态（工作 -> 短休息/长休息，休息 -> 工作）。
- **空闲/暂停状态下的UI更新**: 当番茄钟处于 `STATE_IDLE` 或 `STATE_PAUSED` 时，每秒更新一次UI，以刷新顶部的实时时间。
- **`vTaskDelay`**: 引入短暂延时，避免忙等待，降低CPU负载。

## 五、总结与展望

番茄钟模块提供了一个简单而有效的工具，帮助用户实践番茄工作法。其清晰的状态管理和直观的视觉反馈，使其成为提高专注度和工作效率的实用功能。

未来的改进方向：
1.  **自定义时长**: 允许用户通过UI界面自定义工作和休息的时长。
2.  **会话历史**: 记录并显示每日或每周完成的番茄钟会话数量。
3.  **不同音效**: 允许用户选择不同的结束音效或警告音。
4.  **振动提醒**: 如果设备支持振动马达，可以增加振动提醒功能。
5.  **更丰富的UI**: 增加动画效果，例如时段切换时的过渡动画。

谢谢大家
