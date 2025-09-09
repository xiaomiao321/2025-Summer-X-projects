# 「精準掌控」倒计时模块的设计与实现

## 一、模块概述

倒计时是时钟类应用中一个不可或缺的基础功能。无论是厨房烹饪、运动计时还是工作番茄钟的雏形，一个稳定可靠的倒计时器都扮演着重要角色。本模块的目标是提供一个用户友好、交互直观且功能完备的倒计时应用。

用户可以轻松地通过旋转编码器设置分钟和秒钟，按下按钮启动、暂停或恢复计时。计时过程中，屏幕会实时显示剩余时间（精确到百分之一秒），并辅以一个动态进度条，直观地展示时间流逝。当倒计时结束时，设备会发出持续的蜂鸣声以作提醒。

## 二、实现思路与技术选型

### 核心逻辑：基于`millis()`的状态机

倒计时的核心是精确的时间计算。在Arduino/ESP32环境中，使用`millis()`函数是实现计时功能的最佳实践，因为它不受`delay()`阻塞，并且能提供毫秒级的精度。我们没有使用任何库，而是围绕`millis()`构建了一个简单的状态机来管理倒计时的不同状态。

我们定义了几个关键的状态变量：
-   `countdown_running` (bool): 计时器是否正在运行。
-   `countdown_paused` (bool): 计时器是否被暂停。
-   `countdown_target_millis` (unsigned long): 计时器应结束的目标`millis()`时间戳。

当用户点击“开始”时，程序会记录下当前`millis()`值（`start_millis`），并计算出目标结束时间：`countdown_target_millis = start_millis + duration_in_millis`。在主循环中，我们只需不断地用`countdown_target_millis`减去当前的`millis()`值，就能得到精确的剩余时间。

### 交互设计：分步设置与清晰反馈

为了让用户能方便地设置任意时长，我们将设置过程分成了三个步骤，通过一个枚举类型`CountdownSettingMode`来管理：
1.  `MODE_MINUTES`: 调节分钟。此时旋转编码器会以60秒为单位增减总时长。
2.  `MODE_SECONDS`: 调节秒钟。此时旋转编码器以1秒为单位增减。
3.  `MODE_READY_TO_START`: 准备就绪。此时屏幕上会出现一个醒目的“START”按钮。

用户通过短按按钮在这三种模式间循环切换。在设置分钟或秒钟时，被设置的数字会高亮显示（通过背景色反白），为用户提供了清晰的视觉焦点。这种分步设置的方式避免了在一个界面上用复杂操作控制两个变量的混乱，使得交互流程非常线性、易于理解。

## 三、代码深度解读

### 状态管理与时间计算 (`Countdown.cpp`)

```cpp
// --- State Variables ---
static unsigned long countdown_target_millis = 0;
static unsigned long countdown_start_millis = 0;
static bool countdown_running = false;
static bool countdown_paused = false;

// --- Logic when timer IS running or paused ---
if (button_pressed) {
    if (countdown_running) { // PAUSE
        countdown_pause_time = millis();
        countdown_running = false;
        countdown_paused = true;
    } else { // RESUME from pause
        countdown_start_millis += (millis() - countdown_pause_time);
        countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
        countdown_running = true;
        countdown_paused = false;
    }
}

// --- Update display if running ---
if (countdown_running) {
    unsigned long current_millis = millis();
    long millis_left = countdown_target_millis - current_millis;
    if (millis_left < 0) millis_left = 0;
    // ...
}
```

这段代码是倒计时模块状态切换和时间计算的核心。当按钮被按下时，它在“运行”和“暂停”状态间切换布尔标志位。暂停的实现很简单，只需记录暂停的时刻`countdown_pause_time`。巧妙之处在于**恢复运行的逻辑**：`countdown_start_millis += (millis() - countdown_pause_time)`。这里，我们将暂停所经过的时间补偿回初始的开始时间戳，相当于将整个计时过程的时间轴向后平移了“暂停的这段时长”，从而保证了总倒计时长不变。这是一个非常高效且精确的处理暂停/恢复的技巧。

在`if (countdown_running)`块内，通过`countdown_target_millis - millis()`计算剩余时间，这是整个模块最核心的计算，它保证了计时的准确性，不受其他程序执行时间的影响。

### UI绘制与视觉反馈 (`Countdown.cpp`)

```cpp
void displayCountdownTime(unsigned long millis_left) {
    // ... 时间格式化计算 ...

    // --- Draw Minutes and Seconds with Highlighting ---
    if (!countdown_running && !countdown_paused && countdown_setting_mode == MODE_MINUTES) {
        menuSprite.setTextColor(TFT_BLACK, TFT_YELLOW); // 高亮分钟
    } else {
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    sprintf(buf, "%02ld", minutes);
    menuSprite.drawString(buf, current_x, y_pos);

    // ... 类似逻辑绘制秒钟 ...

    // --- Draw Progress Bar ---
    float progress = (countdown_duration_seconds > 0) ? 1.0 - (float)millis_left / (countdown_duration_seconds * 1000.0) : 0.0;
    // ...
    menuSprite.fillRect(22, ..., (int)((menuSprite.width() - 44) * progress), 16, TFT_GREEN);

    menuSprite.pushSprite(0, 0);
}
```

`displayCountdownTime`函数负责将给定的剩余毫秒数可视化。它不仅显示数字，还承担了重要的交互反馈任务。通过判断当前的`countdown_setting_mode`，它能决定是否要用反色高亮分钟或秒钟的显示，为用户提供清晰的视觉引导。

进度条的实现则是一个简单的数学映射。`progress`变量通过 `(float)millis_left / (countdown_duration_seconds * 1000.0)` 计算出剩余时间的百分比，再反转（`1.0 - ...`）得到已过时间的百分比。最后，将这个0.0到1.0的浮点数乘以进度条的总像素宽度，就得到了当前应填充的像素长度。这个进度条为用户提供了一个比数字更直观的时间感知方式。

## 四、效果展示与体验优化

最终实现的倒计时功能界面清晰，交互流畅。分钟和秒钟的分别设置使得操作精准不易出错。运行中的进度条和精确到0.01秒的计时器，赋予了其专业感。在最后5秒，每秒一次的提示音，以及倒计时结束时响亮的长音，都提供了必要且及时的听觉反馈，确保用户不会错过重要的时间点。

**[此处应有实际效果图，分别展示设置分钟、设置秒钟、准备开始、正在运行和计时结束的界面]**

## 五、总结与展望

倒计时模块通过一个简洁的状态机和对`millis()`函数的精确运用，成功实现了一个功能完整、体验良好的核心应用。它的代码结构清晰，将计时逻辑、用户输入处理和UI显示分离，易于理解和维护。

未来的可扩展方向可以是：
-   **预设时长**：增加一个预设时长列表（如1分钟、5分钟、10分钟），让用户可以一键选择常用倒计时。
-   **循环计时**：增加一个“自动重复”选项，当一次倒计时结束后能自动开始下一次。
-   **更丰富的结束提醒**：除了蜂鸣器，还可以联动LED模块，用灯光闪烁作为提醒，或者播放一段更复杂的提示音乐。