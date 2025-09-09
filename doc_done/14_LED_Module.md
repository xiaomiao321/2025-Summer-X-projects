# LED控制模块：炫彩灯光调节

## 一、引言

LED控制模块旨在为多功能时钟提供一个交互式的RGB LED灯带控制界面。用户可以通过旋转编码器和按键，方便地调节LED灯带的亮度、颜色（色相），并实时在屏幕上看到反馈。本模块旨在提供一个直观、灵活的灯光控制功能，为设备增添个性化色彩。

## 二、实现思路与技术选型

### 2.1 Adafruit NeoPixel库

本模块使用 `Adafruit_NeoPixel` 库来驱动RGB LED灯带（如WS2812B）。该库提供了简单易用的API来设置每个LED的颜色、亮度，并支持多种颜色格式（如RGB、HSV）。

### 2.2 控制模式切换

模块定义了两种主要的控制模式：
- `BRIGHTNESS_MODE`: 在此模式下，旋转编码器用于调节LED灯带的整体亮度。
- `COLOR_MODE`: 在此模式下，旋转编码器用于调节LED灯带的颜色（色相）。

用户可以通过单击按键在这两种模式之间进行切换。

### 2.3 用户界面与视觉反馈

模块提供了清晰的UI界面，实时反馈用户的操作：
- **模式指示**: 当前激活的控制模式（亮度或颜色）会在屏幕上高亮显示。
- **亮度条**: 一个水平进度条直观地显示当前亮度级别。
- **颜色条**: 一个水平进度条显示当前色相，并以实际颜色填充，让用户直接看到颜色变化。
- **操作提示**: 屏幕下方显示操作说明，如“Click: Toggle | Dbl-Click: Exit”。

### 2.4 用户交互

- **旋转编码器**: 
    - 在亮度模式下，旋转编码器用于增加或减少亮度值。
    - 在颜色模式下，旋转编码器用于增加或减少色相值。
- **按键**: 
    - **单击**: 切换亮度模式和颜色模式。
    - **双击**: 退出LED控制模块。

### 2.5 双击检测

模块实现了一个简单的双击检测逻辑 `led_detectDoubleClick()`，通过判断两次单击之间的时间间隔来区分单击和双击。

### 2.6 与其他模块的集成

- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，LED控制模块能够及时退出，避免功能冲突。

## 三、代码展示

### `LED.cpp`

```c++
#include "LED.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include <TFT_eSPI.h>
#include "Menu.h"
#include "MQTT.h" // For access to menuSprite
#include <Adafruit_NeoPixel.h>

// Initialize the NeoPixel strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Enum for control modes
enum ControlMode { BRIGHTNESS_MODE, COLOR_MODE };

// Helper function to detect a double click
bool led_detectDoubleClick() {
    static unsigned long lastClickTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastClickTime > 50 && currentTime - lastClickTime < 400) {
        lastClickTime = 0; // Reset for the next double click
        return true;
    }
    lastClickTime = currentTime;
    return false;
}

// Redesigned UI drawing function using menuSprite
void drawLedControl(uint8_t brightness, uint16_t hue, ControlMode currentMode) {
    menuSprite.fillSprite(TFT_BLACK); // Clear sprite to prevent artifacts
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2);
    menuSprite.setTextDatum(TL_DATUM);

    // --- Brightness Control ---
    menuSprite.drawString("Brightness", 20, 20);
    if (currentMode == BRIGHTNESS_MODE) {
        menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
        menuSprite.drawString("<", 200, 20); // Active indicator
    }
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    int brightnessBarWidth = map(brightness, 0, 255, 0, 200);
    menuSprite.fillRect(20, 50, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 50, brightnessBarWidth, 20, TFT_YELLOW);

    // --- Color Control ---
    menuSprite.drawString("Color", 20, 90);
    if (currentMode == COLOR_MODE) {
        menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
        menuSprite.drawString("<", 200, 90); // Active indicator
    }
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    int hueBarWidth = map(hue, 0, 65535, 0, 200);
    uint32_t color = strip.ColorHSV(hue);
    uint16_t r = (color >> 16) & 0xFF;
    uint16_t g = (color >> 8) & 0xFF;
    uint16_t b = color & 0xFF;
    uint16_t barColor = tft.color565(r, g, b);
    menuSprite.fillRect(20, 120, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 120, hueBarWidth, 20, barColor);

    // --- Instructions ---
    menuSprite.setTextDatum(BC_DATUM);
    menuSprite.setTextSize(1);
    menuSprite.drawString("Click: Toggle | Dbl-Click: Exit", 120, 230);
}

void LEDMenu() {
    initRotaryEncoder();

    ControlMode currentMode = BRIGHTNESS_MODE;
    uint8_t brightness = strip.getBrightness();
    if (brightness == 0) { // If LEDs were off, start at a visible brightness
        brightness = 128;
    }
    uint16_t hue = 0; // Start with red

    // Initial setup
    strip.setBrightness(brightness);
    strip.fill(strip.ColorHSV(hue));
    strip.show();

    // Initial draw
    drawLedControl(brightness, hue, currentMode);
    menuSprite.pushSprite(0, 0);

    bool needsRedraw = true;

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            break;
        }
        if (g_alarm_is_ringing) { break; } // ADDED LINE
        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            needsRedraw = true;
            if (currentMode == BRIGHTNESS_MODE) {
                // Increased sensitivity for brightness
                int newBrightness = brightness + (encoderChange * 10); 
                if (newBrightness < 0) newBrightness = 0;
                if (newBrightness > 255) newBrightness = 255;
                brightness = newBrightness;
                strip.setBrightness(brightness);
            } else { // COLOR_MODE
                // Increased sensitivity for color
                int newHue = hue + (encoderChange * 2048); 
                if (newHue < 0) newHue = 65535 + newHue;
                hue = newHue % 65536;
            }
        }

        if (readButton()) {
            needsRedraw = true;
            if (led_detectDoubleClick()) {
                break; // Exit on double click
            } else {
                // Toggle mode on single click
                currentMode = (currentMode == BRIGHTNESS_MODE) ? COLOR_MODE : BRIGHTNESS_MODE;
            }
        }

        if (needsRedraw) {
            strip.fill(strip.ColorHSV(hue));
            strip.show();
            drawLedControl(brightness, hue, currentMode);
            menuSprite.pushSprite(0, 0); // Push the complete frame to the screen
            needsRedraw = false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

## 四、代码解读

### 4.1 NeoPixel初始化与控制模式

```c++
// Initialize the NeoPixel strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Enum for control modes
enum ControlMode { BRIGHTNESS_MODE, COLOR_MODE };
```
- `strip`: `Adafruit_NeoPixel` 类的全局实例，用于控制LED灯带。`NUM_LEDS` 和 `LED_PIN` 定义了LED的数量和连接引脚。
- `ControlMode`: 枚举类型，定义了两种控制模式：亮度调节和颜色（色相）调节。

### 4.2 双击检测 `led_detectDoubleClick()`

```c++
bool led_detectDoubleClick() {
    static unsigned long lastClickTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastClickTime > 50 && currentTime - lastClickTime < 400) {
        lastClickTime = 0; // Reset for the next double click
        return true;
    }
    lastClickTime = currentTime;
    return false;
}
```
- 此函数用于检测按键的双击事件。它通过比较当前点击时间与上次点击时间的时间间隔来判断是否为双击。
- 如果两次点击间隔在50毫秒到400毫秒之间，则认为是双击。

### 4.3 UI绘制 `drawLedControl()`

```c++
void drawLedControl(uint8_t brightness, uint16_t hue, ControlMode currentMode) {
    menuSprite.fillSprite(TFT_BLACK); // Clear sprite to prevent artifacts
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2);
    menuSprite.setTextDatum(TL_DATUM);

    // --- Brightness Control ---
    menuSprite.drawString("Brightness", 20, 20);
    if (currentMode == BRIGHTNESS_MODE) {
        menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
        menuSprite.drawString("<", 200, 20); // Active indicator
    }
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    int brightnessBarWidth = map(brightness, 0, 255, 0, 200);
    menuSprite.fillRect(20, 50, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 50, brightnessBarWidth, 20, TFT_YELLOW);

    // --- Color Control ---
    menuSprite.drawString("Color", 20, 90);
    if (currentMode == COLOR_MODE) {
        menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
        menuSprite.drawString("<", 200, 90); // Active indicator
    }
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    int hueBarWidth = map(hue, 0, 65535, 0, 200);
    uint32_t color = strip.ColorHSV(hue);
    uint16_t r = (color >> 16) & 0xFF;
    uint16_t g = (color >> 8) & 0xFF;
    uint16_t b = color & 0xFF;
    uint16_t barColor = tft.color565(r, g, b);
    menuSprite.fillRect(20, 120, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 120, hueBarWidth, 20, barColor);

    // --- Instructions ---
    menuSprite.setTextDatum(BC_DATUM);
    menuSprite.setTextSize(1);
    menuSprite.drawString("Click: Toggle | Dbl-Click: Exit", 120, 230);
}
```
- 此函数负责绘制LED控制界面的UI。
- **双缓冲**: 使用 `menuSprite` 进行绘制，避免闪烁。
- **模式指示**: 根据 `currentMode` 高亮显示“Brightness”或“Color”旁边的指示符。
- **亮度条**: 绘制一个灰色背景的矩形条，并根据 `brightness` 值填充一个黄色矩形，表示当前亮度。
- **颜色条**: 绘制一个灰色背景的矩形条，并根据 `hue` 值计算出对应的RGB颜色，然后用该颜色填充一个矩形，表示当前色相。
- **操作提示**: 屏幕下方显示操作说明。

### 4.4 主LED菜单函数 `LEDMenu()`

```c++
void LEDMenu() {
    initRotaryEncoder();

    ControlMode currentMode = BRIGHTNESS_MODE;
    uint8_t brightness = strip.getBrightness();
    if (brightness == 0) { // If LEDs were off, start at a visible brightness
        brightness = 128;
    }
    uint16_t hue = 0; // Start with red

    // Initial setup
    strip.setBrightness(brightness);
    strip.fill(strip.ColorHSV(hue));
    strip.show();

    // Initial draw
    drawLedControl(brightness, hue, currentMode);
    menuSprite.pushSprite(0, 0);

    bool needsRedraw = true;

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            break;
        }
        if (g_alarm_is_ringing) { break; } // ADDED LINE
        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            needsRedraw = true;
            if (currentMode == BRIGHTNESS_MODE) {
                // Increased sensitivity for brightness
                int newBrightness = brightness + (encoderChange * 10); 
                if (newBrightness < 0) newBrightness = 0;
                if (newBrightness > 255) newBrightness = 255;
                brightness = newBrightness;
                strip.setBrightness(brightness);
            } else { // COLOR_MODE
                // Increased sensitivity for color
                int newHue = hue + (encoderChange * 2048); 
                if (newHue < 0) newHue = 65535 + newHue;
                hue = newHue % 65536;
            }
        }

        if (readButton()) {
            needsRedraw = true;
            if (led_detectDoubleClick()) {
                break; // Exit on double click
            } else {
                // Toggle mode on single click
                currentMode = (currentMode == BRIGHTNESS_MODE) ? COLOR_MODE : BRIGHTNESS_MODE;
            }
        }

        if (needsRedraw) {
            strip.fill(strip.ColorHSV(hue));
            strip.show();
            drawLedControl(brightness, hue, currentMode);
            menuSprite.pushSprite(0, 0); // Push the complete frame to the screen
            needsRedraw = false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```
- `LEDMenu()` 是LED控制模块的主入口函数。
- **初始化**: 初始化旋转编码器，设置初始亮度（如果LED关闭则设置为可见亮度）和色相。
- **LED设置**: 使用 `strip.setBrightness()` 和 `strip.fill()` 设置LED灯带的初始状态。
- **主循环**: 持续运行，直到满足退出条件。
- **退出条件**: 
    - `exitSubMenu`: 由主菜单触发的退出标志。
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `led_detectDoubleClick()`: 用户双击按键退出。
- **交互逻辑**: 
    - 旋转编码器：根据当前模式调整亮度或色相。
    - 单击按键：切换控制模式。
- **UI更新**: 只有当亮度、色相或模式发生变化时，才重新绘制UI和更新LED灯带，以提高效率。

## 五、总结与展望

LED控制模块提供了一个直观且功能完善的灯光调节界面，通过旋转编码器和按键的组合，实现了亮度与色相的精细控制。其双击退出和实时视觉反馈提升了用户体验。

未来的改进方向：
1.  **更多颜色模式**: 增加RGB、HSL等其他颜色调节模式。
2.  **预设灯光效果**: 增加多种预设的灯光模式，如呼吸灯、流水灯、闪烁等，并允许用户切换。
3.  **动画速度控制**: 对于某些灯光效果，可以增加动画速度的调节选项。
4.  **场景模式**: 允许用户保存和加载不同的灯光场景设置。
5.  **与音乐联动**: 实现LED灯光效果与音乐节奏或频率的联动。

谢谢大家
