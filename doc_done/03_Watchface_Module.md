# 表盘模块：多样化时间与信息展示

## 一、引言

表盘模块是多功能时钟的用户界面核心，它负责以各种视觉风格展示时间、日期、天气等关键信息。本模块旨在提供一系列预设的表盘设计，用户可以通过交互切换，以满足不同的审美和信息需求。每个表盘都经过精心设计，以确保在有限的屏幕空间内清晰、美观地呈现数据。

## 二、实现思路与技术选型

### 2.1 统一的绘制画布

所有表盘的绘制都基于 `TFT_eSprite` 对象 `menuSprite`。这种双缓冲机制是避免屏幕闪烁的关键。所有图形和文本操作都先在内存中的 `menuSprite` 上完成，然后通过 `menuSprite.pushSprite(0, 0)` 一次性将完整的画面推送到物理屏幕上，从而提供流畅的视觉体验。

### 2.2 数据来源

表盘所需的时间和日期信息主要来源于全局的 `timeinfo` 结构体，该结构体由NTP时间同步模块 (`weather.cpp`) 负责更新。天气信息则来源于全局的 `weatherData` 结构体，同样由 `weather.cpp` 模块负责获取和更新。

### 2.3 模块化设计

每个表盘都被封装在一个独立的函数中（例如 `drawWatchface1`），这使得表盘的添加、修改和维护变得简单。主程序只需调用相应的表盘函数即可切换显示。

### 2.4 优化绘制效率

为了提高刷新率和减少CPU负载，表盘的绘制逻辑通常会尽量只更新发生变化的部分，而不是每次都重绘整个屏幕。例如，数字时钟只在秒数变化时更新秒的显示区域，而不是整个时间字符串。

## 三、代码展示与解读

`Watchface.cpp` 包含了多个表盘的实现。下面将逐一介绍每个表盘的设计思路和关键代码。

### 3.1 表盘 1：简洁数字时钟

#### 设计思路

这个表盘以大字体居中显示时分秒，并在下方显示日期和星期。设计目标是简洁、直观，让用户一眼就能获取到最核心的时间信息。

#### 代码片段

```c++
#include "Watchface.h"
#include <TFT_eSPI.h>
#include "weather.h" // For timeinfo and weatherData
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Alarm.h"

// Watchface 1: Simple Digital Clock
void drawWatchface1() {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextDatum(MC_DATUM); // Center alignment

  // Display Time (HH:MM:SS)
  menuSprite.setTextFont(7); // Large font
  menuSprite.setTextSize(1);
  menuSprite.setTextColor(TFT_WHITE);
  char time_buf[9];
  strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo);
  menuSprite.drawString(time_buf, 120, 80);

  // Display Date (YYYY-MM-DD)
  menuSprite.setTextFont(1); // Smaller font
  menuSprite.setTextSize(2);
  char date_buf[16];
  strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);
  menuSprite.drawString(date_buf, 120, 120);

  // Display Weekday (e.g., Mon, Tue)
  char wday_buf[8];
  strftime(wday_buf, sizeof(wday_buf), "%a", &timeinfo);
  menuSprite.drawString(wday_buf, 120, 150);

  // Display Weather Info
  if (weatherData.valid) {
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_CYAN);
    menuSprite.drawString(weatherData.city, 120, 180);

    menuSprite.setTextColor(TFT_YELLOW);
    menuSprite.drawString(weatherData.description, 120, 200);

    menuSprite.setTextColor(TFT_ORANGE);
    char temp_buf[10];
    sprintf(temp_buf, "%.1f C", weatherData.temp);
    menuSprite.drawString(temp_buf, 120, 220);
  } else {
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_RED);
    menuSprite.drawString("Weather N/A", 120, 200);
  }
  menuSprite.pushSprite(0, 0);
}
```

#### 代码解读

- `menuSprite.fillScreen(TFT_BLACK)`: 每次绘制前清空画布，确保画面干净。
- `menuSprite.setTextDatum(MC_DATUM)`: 设置文本基准点为中间中心，方便使用坐标 `(120, Y)` 进行居中绘制。
- `strftime` 函数: 用于将 `timeinfo` 结构体中的时间信息格式化为字符串，例如 `%H:%M:%S` 格式化为时分秒，`%Y-%m-%d` 格式化为年月日，`%a` 格式化为星期几的缩写。
- `menuSprite.drawString`: 将格式化后的时间、日期、星期字符串绘制到指定位置。
- 天气信息: 根据 `weatherData.valid` 标志判断天气数据是否有效。如果有效，则显示城市、天气描述和温度；否则显示“Weather N/A”。
- `menuSprite.pushSprite(0, 0)`: 将绘制好的内容一次性推送到屏幕。

### 3.2 表盘 2：模拟时钟（待实现）

#### 设计思路

该表盘将以模拟时钟的形式展示时间，可能包含时针、分针、秒针，并辅以刻度盘。下方可以保留数字日期和天气信息。设计目标是提供一种更具传统美感的时钟界面。

#### 代码片段

```c++
// Watchface 2: Analog Clock (Placeholder)
void drawWatchface2() {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextDatum(MC_DATUM);
  menuSprite.setTextSize(2);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.drawString("Analog Clock", 120, 100);
  menuSprite.drawString("Coming Soon!", 120, 140);
  menuSprite.pushSprite(0, 0);
}
```

#### 代码解读

- 这是一个占位符函数，目前只显示“Analog Clock”和“Coming Soon!”。实际实现中，需要计算时针、分针、秒针的坐标，并使用 `menuSprite.drawLine` 或其他图形函数绘制。

### 3.3 表盘 3：天气中心表盘（待实现）

#### 设计思路

该表盘将天气信息作为主要显示内容，可能包含更大的天气图标、更详细的天气描述（如体感温度、风速等），时间信息则以较小字体显示在角落。设计目标是让用户快速获取天气概览。

#### 代码片段

```c++
// Watchface 3: Weather Centric (Placeholder)
void drawWatchface3() {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextDatum(MC_DATUM);
  menuSprite.setTextSize(2);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.drawString("Weather Centric", 120, 100);
  menuSprite.drawString("Coming Soon!", 120, 140);
  menuSprite.pushSprite(0, 0);
}
```

#### 代码解读

- 同样是一个占位符。实际实现中，需要重点突出天气图标和描述，可能需要加载自定义的天气图标图片。

### 3.4 表盘 4：极简主义表盘（待实现）

#### 设计思路

该表盘将只显示最基本的时间信息，可能只有时和分，去除秒、日期、天气等所有额外元素，以追求极致的简洁和纯粹。设计目标是提供一个无干扰、专注于时间的界面。

#### 代码片段

```c++
// Watchface 4: Minimalist (Placeholder)
void drawWatchface4() {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextDatum(MC_DATUM);
  menuSprite.setTextSize(2);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.drawString("Minimalist", 120, 100);
  menuSprite.drawString("Coming Soon!", 120, 140);
  menuSprite.pushSprite(0, 0);
}
```

#### 代码解读

- 占位符。实际实现中，需要精简显示内容，可能只显示时和分，并选择一种优雅的字体。

### 3.5 表盘 5：信息密度表盘（待实现）

#### 设计思路

该表盘旨在在一个屏幕上尽可能多地展示信息，除了时间、日期、天气，可能还包括温度、湿度、气压、WiFi信号强度、电池电量等。设计目标是为需要全面了解设备状态的用户提供一个“仪表盘”式的界面。

#### 代码片段

```c++
// Watchface 5: Information Dense (Placeholder)
void drawWatchface5() {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextDatum(MC_DATUM);
  menuSprite.setTextSize(2);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.drawString("Information Dense", 120, 100);
  menuSprite.drawString("Coming Soon!", 120, 140);
  menuSprite.pushSprite(0, 0);
}
```

#### 代码解读

- 占位符。实际实现中，需要合理布局，可能使用更小的字体和图标，并从其他模块获取更多系统状态数据。

## 四、主菜单集成 `weatherMenu()`

`weatherMenu()` 函数是 `weather.cpp` 模块中的主入口，它不仅负责显示天气和时间，还集成了表盘切换的逻辑。当用户在主菜单中选择“Clock”功能时，会进入 `weatherMenu`。在该函数内部，通过旋转编码器可以切换不同的表盘。

```c++
void weatherMenu() {
  // ... (时间同步和天气更新逻辑)

  // 初始绘制第一个表盘
  currentWatchface = 0; // 默认显示第一个表盘
  drawWatchface(currentWatchface); // 调用通用的绘制函数

  while (true) {
    // ... (退出子菜单和闹钟响铃检查)

    // 处理旋转编码器切换表盘
    int encoderChange = readEncoder();
    if (encoderChange != 0) {
      currentWatchface = (currentWatchface + encoderChange + NUM_WATCHFACES) % NUM_WATCHFACES;
      drawWatchface(currentWatchface); // 切换表盘并重新绘制
      tone(BUZZER_PIN, 1000, 20); // 播放切换音效
    }

    // ... (时间显示更新和天气更新)

    // 处理按键事件（例如，单击退出）
    if (readButton()) {
      tone(BUZZER_PIN, 1500, 50);
      break; // 退出循环，返回主菜单
    }
    // ... (长按事件)

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// 通用表盘绘制函数
void drawWatchface(int index) {
  switch(index) {
    case 0: drawWatchface1(); break;
    case 1: drawWatchface2(); break;
    case 2: drawWatchface3(); break;
    case 3: drawWatchface4(); break;
    case 4: drawWatchface5(); break;
    default: drawWatchface1(); break; // Fallback
  }
}
```

#### 代码解读

- `currentWatchface`: 全局变量，记录当前正在显示的表盘索引。
- `NUM_WATCHFACES`: 定义了总共有多少个表盘。
- `drawWatchface(int index)`: 一个通用的函数，根据传入的索引调用对应的表盘绘制函数。这使得表盘的切换逻辑更加简洁。
- 旋转编码器交互: 当检测到旋转编码器转动时，`currentWatchface` 会根据转动方向进行增减，并通过取模运算 (`% NUM_WATCHFACES`) 确保索引在有效范围内循环。然后调用 `drawWatchface` 重新绘制新的表盘。
- 按键交互: 单击按键会退出 `weatherMenu`，返回主菜单。

## 五、总结与展望

表盘模块通过模块化的设计和双缓冲技术，为多功能时钟提供了多样化且流畅的界面展示。虽然目前大部分表盘仍是占位符，但其架构已经为未来的扩展奠定了基础。

未来的改进方向：
1.  **完成所有表盘的实现**: 按照设计思路，逐步实现模拟时钟、天气中心、极简主义和信息密度表盘。
2.  **表盘自定义**: 允许用户调整表盘的颜色、字体、显示元素等。
3.  **动画效果**: 在表盘切换或特定事件发生时，加入平滑的动画效果，提升用户体验。
4.  **性能优化**: 对于复杂的表盘，可以进一步优化绘制逻辑，例如只更新变化区域，或使用更高效的图形算法。
5.  **更多表盘**: 设计和实现更多独特风格的表盘，满足不同用户的个性化需求。

谢谢大家
