# 动画模块：随机视觉与听觉盛宴

## 一、引言

动画模块旨在为多功能时钟提供一个充满趣味和随机性的视觉与听觉体验。它能够生成各种随机的图形动画，并伴随蜂鸣器发出的随机音效，同时驱动LED灯带显示与动画颜色同步的灯光效果。本模块旨在展示设备的图形和声音能力，为用户提供一个轻松愉快的互动环节。

## 二、实现思路与技术选型

### 2.1 随机动画生成

模块的核心是其随机性。它会随机生成：
- **图形类型**: 例如，使用 `drawSmoothArc` 绘制平滑的圆弧。
- **颜色**: 随机生成前景颜色和背景颜色。
- **位置**: 随机生成图形的X、Y坐标。
- **尺寸**: 随机生成图形的半径和厚度。
- **角度**: 随机生成圆弧的起始和结束角度。

这种高度的随机性确保了每次进入动画模块都能看到不同的视觉效果。

### 2.2 多感官联动

- **视觉**: 利用 `TFT_eSPI` 库在屏幕上绘制随机生成的图形。
- **听觉**: 通过蜂鸣器播放随机频率和持续时间的音效，与视觉动画同步。
- **灯光**: 利用 `Adafruit_NeoPixel` 库控制LED灯带。LED灯的颜色会根据当前屏幕动画的颜色进行同步，增强沉浸感。

### 2.3 FreeRTOS任务管理

动画的生成和播放逻辑在一个独立的FreeRTOS任务 `Animation_task` 中运行。这确保了动画的流畅性，并且不会阻塞主循环或其他任务。任务会持续运行，直到接收到停止信号。

### 2.4 任务控制与退出

- **停止标志**: `stopAnimationTask` 是一个 `volatile bool` 类型的全局标志，用于通知 `Animation_task` 停止运行。当用户退出动画模块或闹钟响铃时，此标志会被设置为 `true`。
- **任务自删除**: `Animation_task` 在检测到停止标志后，会执行清理操作（停止蜂鸣器、清空LED灯带），然后自行删除任务，释放资源。

### 2.5 动画速度控制

动画的播放速度会逐渐加快，从初始的500毫秒延迟逐渐减少到50毫秒，增加了动画的动态变化。

## 三、代码展示

### `animation.cpp`

```c++
#include "animation.h"
#include "Menu.h"
#include "MQTT.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include "Buzzer.h" // For BUZZER_PIN
#include "LED.h" // Include LED.h to use the global strip object

// Global flag to signal task to stop
volatile bool stopAnimationTask = false;
TaskHandle_t animationTaskHandle = NULL;

void Animation_task(void *pvParameters)
{
  int delay_ms = 500;

  while(!stopAnimationTask) // Check the flag
  {
    // 1. Draw the animation frame
    uint16_t fg_color = random(0x10000);
    uint16_t bg_color = TFT_BLACK;
    uint16_t x = random(tft.width());
    uint16_t y = random(tft.height());
    uint8_t radius = random(20, tft.width()/4);
    uint8_t thickness = random(1, radius / 4);
    uint8_t inner_radius = radius - thickness;
    uint16_t start_angle = random(361);
    uint16_t end_angle = random(361);
    bool arc_end = random(2);
    tft.drawSmoothArc(x, y, radius, inner_radius, start_angle, end_angle, fg_color, bg_color, arc_end);

    // 2. Update NeoPixels
    // Convert the 16-bit TFT color (5-6-5) to a 24-bit RGB color for the NeoPixels
    uint8_t r = (fg_color & 0xF800) >> 8;
    uint8_t g = (fg_color & 0x07E0) >> 3;
    uint8_t b = (fg_color & 0x001F) << 3;
    strip.fill(strip.Color(r, g, b)); // Use the global strip object
    strip.show();

    // 3. Play a sound effect using tone() with a duration
    tone(BUZZER_PIN, random(800, 1500), delay_ms);

    // 4. Control animation speed
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    if (delay_ms > 50) {
      delay_ms -= 10;
    }
  }

  // Cleanup before exiting
  noTone(BUZZER_PIN);
  strip.clear();
  strip.show();
  animationTaskHandle = NULL; // Clear the handle
  vTaskDelete(NULL); // Delete self
}

void AnimationMenu()
{
  tft.fillScreen(TFT_BLACK);
  
  // Initialize NeoPixel strip
  strip.show(); // Initialize all pixels to 'off'

  stopAnimationTask = false; // Reset the flag
  
  // Make sure no old task is running
  if(animationTaskHandle != NULL) {
    vTaskDelete(animationTaskHandle);
    animationTaskHandle = NULL;
  }

  xTaskCreatePinnedToCore(Animation_task, "Animation_task", 2048, NULL, 1, &animationTaskHandle, 0);

  initRotaryEncoder();

  while(true)
  {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        if (animationTaskHandle != NULL) {
            stopAnimationTask = true; // Signal the background task to stop
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        if (animationTaskHandle != NULL) {
            stopAnimationTask = true; // Signal the background task to stop
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
        break; // Exit loop
    }
    if(readButton())
    {
      // Signal the task to stop
      if (animationTaskHandle != NULL) {
        stopAnimationTask = true;
      }

      // Wait for the task to delete itself
      vTaskDelay(pdMS_TO_TICKS(200)); 

      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
    // display = 48;
    // picture_flag = 0;
    // showMenuConfig();
}
```

## 四、代码解读

### 4.1 动画任务 `Animation_task()`

```c++
void Animation_task(void *pvParameters)
{
  int delay_ms = 500;

  while(!stopAnimationTask) // Check the flag
  {
    // 1. Draw the animation frame
    uint16_t fg_color = random(0x10000);
    uint16_t bg_color = TFT_BLACK;
    uint16_t x = random(tft.width());
    uint16_t y = random(tft.height());
    uint8_t radius = random(20, tft.width()/4);
    uint8_t thickness = random(1, radius / 4);
    uint8_t inner_radius = radius - thickness;
    uint16_t start_angle = random(361);
    uint16_t end_angle = random(361);
    bool arc_end = random(2);
    tft.drawSmoothArc(x, y, radius, inner_radius, start_angle, end_angle, fg_color, bg_color, arc_end);

    // 2. Update NeoPixels
    // Convert the 16-bit TFT color (5-6-5) to a 24-bit RGB color for the NeoPixels
    uint8_t r = (fg_color & 0xF800) >> 8;
    uint8_t g = (fg_color & 0x07E0) >> 3;
    uint8_t b = (fg_color & 0x001F) << 3;
    strip.fill(strip.Color(r, g, b)); // Use the global strip object
    strip.show();

    // 3. Play a sound effect using tone() with a duration
    tone(BUZZER_PIN, random(800, 1500), delay_ms);

    // 4. Control animation speed
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    if (delay_ms > 50) {
      delay_ms -= 10;
    }
  }

  // Cleanup before exiting
  noTone(BUZZER_PIN);
  strip.clear();
  strip.show();
  animationTaskHandle = NULL; // Clear the handle
  vTaskDelete(NULL); // Delete self
}
```
- 这是一个FreeRTOS任务，负责生成和播放动画。
- **随机图形绘制**: 每次循环都会随机生成颜色、位置、大小和角度，然后使用 `tft.drawSmoothArc` 绘制一个圆弧。`random()` 函数用于生成随机数。
- **LED同步**: 将屏幕上绘制的图形颜色转换为LED灯带的颜色格式，并使用 `strip.fill()` 和 `strip.show()` 更新LED灯带的颜色，使其与屏幕动画同步。
- **随机音效**: 使用 `tone()` 函数在蜂鸣器上播放随机频率和持续时间的音效。
- **动画速度**: `delay_ms` 控制动画帧之间的延迟，并逐渐减小，使动画速度逐渐加快。
- **任务停止与清理**: 循环会检查 `stopAnimationTask` 标志。当此标志为 `true` 时，任务会停止蜂鸣器、清空LED灯带，然后自行删除任务，释放资源。

### 4.2 主动画菜单函数 `AnimationMenu()`

```c++
void AnimationMenu()
{
  tft.fillScreen(TFT_BLACK);
  
  // Initialize NeoPixel strip
  strip.show(); // Initialize all pixels to 'off'

  stopAnimationTask = false; // Reset the flag
  
  // Make sure no old task is running
  if(animationTaskHandle != NULL) {
    vTaskDelete(animationTaskHandle);
    animationTaskHandle = NULL;
  }

  xTaskCreatePinnedToCore(Animation_task, "Animation_task", 2048, NULL, 1, &animationTaskHandle, 0);

  initRotaryEncoder();

  while(true)
  {
    if (exitSubMenu) {
        exitSubMenu = false; // Reset flag
        if (animationTaskHandle != NULL) {
            stopAnimationTask = true; // Signal the background task to stop
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
        break;
    }
    if (g_alarm_is_ringing) { // ADDED LINE
        if (animationTaskHandle != NULL) {
            stopAnimationTask = true; // Signal the background task to stop
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
        break; // Exit loop
    }
    if(readButton())
    {
      // Signal the task to stop
      if (animationTaskHandle != NULL) {
        stopAnimationTask = true;
      }

      // Wait for the task to delete itself
      vTaskDelay(pdMS_TO_TICKS(200)); 

      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
    // display = 48;
    // picture_flag = 0;
    // showMenuConfig();
}
```
- `AnimationMenu()` 是动画模块的主入口函数。
- **初始化**: 清屏，确保LED灯带初始状态为关闭。
- **任务创建**: 重置 `stopAnimationTask` 标志，并创建 `Animation_task` 任务来启动动画。
- **主循环**: 持续运行，直到满足退出条件。
- **退出条件**: 
    - `exitSubMenu`: 由主菜单触发的退出标志。
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `readButton()`: 用户单击按键退出。
- **任务停止**: 退出时，将 `stopAnimationTask` 设置为 `true`，并等待 `Animation_task` 自行删除，确保资源被正确释放。

## 五、总结与展望

动画模块通过结合随机图形、同步LED灯光和随机音效，为用户提供了一个动态且富有创意的互动体验。它充分展示了ESP32的图形和声音处理能力。

未来的改进方向：
1.  **更多动画类型**: 增加更多预设的动画模式，例如粒子效果、几何图形变换、文字动画等。
2.  **用户自定义模式**: 允许用户通过某种方式（例如，通过串口命令或Web界面）定义自己的动画模式和参数。
3.  **音乐同步**: 实现更复杂的音频分析，使动画效果能够更精确地与音乐节奏和频率同步。
4.  **交互式动画**: 允许用户通过旋转编码器或按键实时改变动画的某些参数（如速度、颜色、形状）。
5.  **低功耗模式**: 在动画播放时，优化CPU和屏幕刷新，以降低功耗。

谢谢大家
