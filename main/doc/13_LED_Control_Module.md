# 「光影魔术」NeoPixel LED灯带控制模块

## 一、模块概述

本模块为多功能时钟赋予了“光”的维度。它通过驱动一条**Adafruit NeoPixel**（WS2812B）全彩LED灯带，让用户可以随心所欲地控制灯光的亮度与色彩，为设备增添了丰富的环境氛围和视觉效果。无论是作为桌面摆件的氛围灯，还是作为其他模块（如闹钟、倒计时）的视觉提醒，这个模块都提供了坚实的底层支持。

用户界面非常直观：屏幕上会显示“亮度”和“颜色”两个控制项。用户通过单击按钮在两者之间切换控制焦点，然后通过旋转编码器来实时地、平滑地调节选中的参数。所有调整都会即时地在LED灯带上反映出来，实现了所见即所得的控制体验。

## 二、实现思路与技术选型

### 硬件交互：`Adafruit_NeoPixel`库

与DS18B20模块类似，直接通过GPIO去模拟WS2812B的严格时序协议是非常困难的。因此，我们采用了业界标准的`Adafruit_NeoPixel`库来简化硬件交互。这个库封装了所有底层的复杂操作，让我们能通过非常高级和人性化的API来控制LED。

-   **初始化**：我们首先在全局范围实例化一个`Adafruit_NeoPixel`对象`strip`，并传入LED的数量、引脚号和灯珠类型等参数。
-   **颜色控制**：该库最强大的功能之一是其丰富的颜色表示方法。除了传统的RGB，它还直接支持**HSV/HSB（色相、饱和度、亮度）**颜色模型。在本项目中，我们主要使用`strip.ColorHSV(hue)`。用户只需调节`hue`（色相）这一个0到65535的参数，就能平滑地扫过整个色谱（赤橙黄绿青蓝紫），而无需分别计算R、G、B三个分量。这极大地简化了颜色选择的逻辑。
-   **亮度控制**：通过`strip.setBrightness(brightness)`函数，我们可以独立于颜色，全局性地控制整条灯带的亮度，非常方便。
-   **生效**：所有对颜色和亮度的设置，在调用`strip.show()`函数之前都只是在内存中生效。只有执行了`strip.show()`，这些设置才会被真正地发送到LED灯带上，点亮LED。 (Adafruit的官方教程是学习NeoPixel库的最佳起点：[Adafruit NeoPixel Überguide](https://learn.adafruit.com/adafruit-neopixel-uberguide))

### UI设计与交互逻辑

我们的目标是让用户能用最简单的操作（一个旋钮和一个按钮）来控制两个参数（亮度和颜色）。

-   **模式切换**：我们定义了一个枚举`ControlMode`（`BRIGHTNESS_MODE`, `COLOR_MODE`）来管理当前的控制焦点。用户每次单击按钮，程序就会在这两个模式之间切换。UI上会有一个醒目的黄色`<`符号，来指示当前正在调节的是哪一个参数。
-   **参数调节**：在主循环中，程序会检查旋钮的转动。如果当前是`BRIGHTNESS_MODE`，旋钮的增量就会被用来修改`brightness`变量；如果当前是`COLOR_MODE`，则用来修改`hue`变量。为了提升体验，我们为调节增加了“灵敏度”，旋钮每转动一格，对应的参数值会跳变一个较大的步长（如亮度`*10`，色相`*2048`），让调节过程更高效。
-   **双击退出**：与游戏模块类似，我们为退出操作设计了双击交互，避免了与单击（切换模式）的冲突。

## 三、代码深度解读

### 核心控制循环 (`LED.cpp`)

```cpp
void LEDMenu() {
    // ... 初始化亮度、色相等变量 ...
    ControlMode currentMode = BRIGHTNESS_MODE;

    while (true) {
        // ... 退出逻辑 ...
        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            if (currentMode == BRIGHTNESS_MODE) {
                int newBrightness = brightness + (encoderChange * 10); 
                // ... 边界检查 ...
                brightness = newBrightness;
                strip.setBrightness(brightness);
            } else { // COLOR_MODE
                int newHue = hue + (encoderChange * 2048); 
                // ... 边界检查 ...
                hue = newHue % 65536;
            }
        }

        if (readButton()) {
            if (led_detectDoubleClick()) { break; } // 双击退出
            else { currentMode = (currentMode == BRIGHTNESS_MODE) ? COLOR_MODE : BRIGHTNESS_MODE; } // 单击切换模式
        }

        // 只有在参数变化时才更新LED和屏幕
        if (needsRedraw) {
            strip.fill(strip.ColorHSV(hue));
            strip.show();
            drawLedControl(brightness, hue, currentMode);
            menuSprite.pushSprite(0, 0);
            needsRedraw = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

这段代码是LED控制模块的主循环，清晰地展示了其交互逻辑。它通过`currentMode`来决定编码器的输入应该作用于哪个变量。`strip.setBrightness()`会立刻改变灯带的亮度，而颜色的改变则是在`strip.fill(strip.ColorHSV(hue))`中完成。`strip.fill()`会将整条灯带的所有像素都设置为同一个颜色。最后，`strip.show()`将这些改变发送出去。`needsRedraw`标志位是一个简单的优化，它确保了只有在用户操作导致参数变化时，程序才会去更新LED和刷新屏幕，避免了在空闲时进行不必要的计算和通信。

### UI绘制函数 (`LED.cpp`)

```cpp
void drawLedControl(uint8_t brightness, uint16_t hue, ControlMode currentMode) {
    menuSprite.fillSprite(TFT_BLACK);
    // ...

    // --- Brightness Control ---
    if (currentMode == BRIGHTNESS_MODE) { menuSprite.drawString("<", 200, 20); }
    int brightnessBarWidth = map(brightness, 0, 255, 0, 200);
    menuSprite.fillRect(20, 50, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 50, brightnessBarWidth, 20, TFT_YELLOW);

    // --- Color Control ---
    if (currentMode == COLOR_MODE) { menuSprite.drawString("<", 200, 90); }
    int hueBarWidth = map(hue, 0, 65535, 0, 200);
    uint32_t color = strip.ColorHSV(hue);
    // ... 颜色转换 ...
    uint16_t barColor = tft.color565(r, g, b);
    menuSprite.fillRect(20, 120, 200, 20, TFT_DARKGREY);
    menuSprite.fillRect(20, 120, hueBarWidth, 20, barColor);

    // ...
}
```

这个函数负责将当前的亮度和颜色状态可视化。它通过绘制两个水平的进度条来直观地表示参数值。对于颜色条，它会先调用`strip.ColorHSV(hue)`来获取当前色相对应的24位RGB颜色，然后将其转换为TFT屏幕可以显示的16位颜色`barColor`，再用这个颜色来填充进度条。这种方式让进度条本身的颜色就能准确地反映当前选中的颜色，非常直观。

## 四、效果展示与体验优化

最终的LED控制模块提供了一个非常流畅和直观的交互体验。用户转动旋钮，灯带的亮度或颜色会毫无延迟地随之变化。界面上的指示器和进度条清晰地反映了当前的状态和数值，使得整个控制过程精准而高效。

**[此处应有实际效果图或GIF，展示用户操作屏幕UI，同时LED灯带颜色和亮度实时变化的效果]**

## 五、总结与展望

LED控制模块是硬件交互和UI设计紧密结合的典范。它通过成熟的库简化了底层驱动，将重心放在了如何设计一个优雅、高效的交互流程上。HSV颜色模型的运用，是简化颜色选择逻辑的关键。这个模块不仅自身是一个有趣的独立功能，也为其他模块（如闹钟、番茄钟、音乐播放器）提供了强大的视觉反馈能力，是提升整个产品“品质感”的重要一环。

未来的可扩展方向可以是：
-   **更多灯光效果**：除了静态的纯色，还可以增加如“彩虹流动”、“呼吸灯”、“闪烁”等多种动态灯光效果，并让用户可以在这些效果之间切换。
-   **饱和度控制**：在HSV模型中，我们只让用户调节了H（色相）。可以增加对S（饱和度）的控制，让用户能在鲜艳的彩色和单调的白色之间进行调节。
-   **与音乐同步**：将此模块与音乐播放器深度整合，让LED灯带的颜色和亮度能根据音乐的频谱或节拍进行实时、复杂的动态变化。