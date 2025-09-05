# 「声色俱佳」音乐播放器与频谱可视化的实现

## 一、模块概述

一个多功能设备怎能缺少娱乐功能？音乐模块为我们的时钟注入了艺术的活力。它不仅仅是一个能播放预置旋律的蜂鸣器，更是一个带有完整交互界面、支持多种播放模式，并集成了酷炫频谱可视化效果的全功能音乐播放器。

用户可以在一个清晰的列表中选择想听的歌曲，播放器启动后，会进入一个专门的播放界面。该界面不仅显示歌曲名、播放/暂停状态、播放模式（列表循环、单曲循环、随机播放），还有一个动态的“频谱”会跟随着音乐的频率跳动。同时，底部的进度条和时间显示也让用户对播放进程了如指掌。

## 二、实现思路与技术选型

### 核心逻辑：FreeRTOS多任务处理

音乐播放和UI刷新是两个必须并行处理的任务。如果在同一个`loop()`中处理，播放音乐时的延时`vTaskDelay()`会冻结整个UI，导致界面卡死、无法响应用户操作。为了解决这个问题，我们采用了**FreeRTOS的多任务**处理机制，这也是在ESP32这类带有操作系统的微控制器上进行复杂应用开发的标准实践。

-   **`Buzzer_Task`**：这是一个专门负责播放音乐的后台任务。它在一个循环中，逐个读取歌曲的音符频率和时长，并调用`tone()`函数驱动蜂鸣器发声。由于它在自己的任务上下文中运行，它的延时不会影响到UI任务。
-   **`BuzzerMenu` (UI任务)**：这是在前台运行的UI主循环。它负责响应用户的按键和旋钮输入（切歌、暂停、切换模式），并定时刷新屏幕，显示播放进度和频谱动画。两个任务通过共享的全局易失性（`volatile`）变量（如`isPaused`, `currentPlayMode`）来进行通信和状态同步。

### 音乐数据表示：PROGMEM与结构体

我们的音乐数据（音符频率数组和时长数组）是预置在代码中的。这些数据是只读的，并且可能占用较大的空间。为了节约宝贵的RAM，我们使用了`PROGMEM`关键字。这会告诉编译器将这些数据存储在Flash（程序存储器）中，而不是在程序启动时加载到RAM里。在需要使用时，我们再通过`memcpy_P`或`pgm_read_word`等专用函数从Flash中读取，实现了空间换时间。(深入了解`PROGMEM`，可以阅读 [Arduino官方参考文档](https://www.arduino.cc/reference/en/language/variables/utilities/progmem/))

我们将每一首歌的相关信息——歌名、旋律数组指针、时长数组指针和长度——都封装在一个`Song`结构体中，并用一个`songs[]`数组来管理所有歌曲。这种结构化的数据管理方式，与菜单系统的设计思想一脉相承，清晰且易于扩展。

### 频谱可视化：模拟与映射

在没有专业音频处理硬件（如FFT加速器）的单片机上，实时计算音频的快速傅里叶变换（FFT）以获得真实频谱，是非常消耗CPU资源且难以做到实时的。因此，我们采用了一种**“模拟频谱”**的取巧方法。

我们实现了一个`generate_fake_spectrum()`函数。它并不进行真实的频谱分析，而是根据当前正在播放的音符频率，在预设的16个频段（`NUM_BANDS`）中，找到该频率对应的频段，然后人为地给这个频段和它相邻的频段赋予一个较高的“能量值”。例如，如果当前音符是440Hz（中央A），它可能落在第8个频段，我们就让第8个频段的柱子最高，第7和第9个频段的柱子稍矮。这种方法虽然不是科学精确的，但它能产生与音乐频率相关的、视觉上看起来非常酷炫的动态效果，以极低的性能开销实现了“看起来像”频谱的功能。

## 三、代码深度解读

### 音乐播放后台任务 (`Buzzer.cpp`)

```cpp
void Buzzer_Task(void *pvParameters) {
  int songIdx = *(int*)pvParameters;
  for(;;) { // 任务主循环，用于处理播放模式切换
    shared_song_index = songIdx;
    Song song;
    memcpy_P(&song, &songs[songIdx], sizeof(Song)); // 从PROGMEM加载歌曲信息

    for (int i = 0; i < song.length; i++) { // 播放一首歌的循环
      shared_note_index = i;
      if (stopBuzzerTask) { vTaskDelete(NULL); } // 检查退出标志

      while (isPaused) { vTaskDelay(pdMS_TO_TICKS(50)); } // 暂停逻辑

      int note = pgm_read_word(song.melody+i); // 从PROGMEM读取音符
      int duration = pgm_read_word(song.durations+i); // 从PROGMEM读取时长
      
      generate_fake_spectrum(note); // 生成模拟频谱
      if (note > 0) { tone(BUZZER_PIN, note, duration*0.9); }
      vTaskDelay(pdMS_TO_TICKS(duration));
    }

    // 根据播放模式决定下一首歌的索引
    if (currentPlayMode == LIST_LOOP) { songIdx = (songIdx + 1) % numSongs; }
    // ... 其他模式逻辑 ...
  }
}
```

这是音乐播放的核心。它是一个独立的FreeRTOS任务，通过`xTaskCreatePinnedToCore`启动。代码的关键点在于：
-   **双层循环**：外层`for(;;)`循环负责处理歌曲间的切换（如列表循环、随机播放），内层`for`循环负责播放一首歌内的所有音符。
-   **PROGMEM读取**：`memcpy_P`和`pgm_read_word`是与`PROGMEM`配套使用的函数，用于安全地从Flash程序存储器中读取数据到内存变量。
-   **暂停实现**：`while (isPaused)`是一个阻塞式的暂停。当UI任务将全局变量`isPaused`设为`true`时，这个循环会一直空转，从而“卡”住播放进程，实现了暂停。当`isPaused`被设回`false`时，循环退出，音乐继续。
-   **任务间通信**：`shared_song_index`, `shared_note_index`, `isPaused`等都是`volatile`全局变量，它们是UI任务和播放任务之间进行状态同步和信息传递的桥梁。

### UI与播放进度的同步 (`Buzzer.cpp`)

```cpp
static uint32_t calculateElapsedTime_ms() {
    if (shared_song_index < 0 || shared_song_index >= numSongs) return 0;
    Song song;
    memcpy_P(&song, &songs[shared_song_index], sizeof(Song));
    uint32_t elapsed_ms = 0;
    // 累加所有已播放音符的总时长
    for (int i = 0; i < shared_note_index; i++) {
        elapsed_ms += pgm_read_word(song.durations + i);
    }
    // 加上当前正在播放的音符所经过的时间
    TickType_t current_note_elapsed_ticks = xTaskGetTickCount() - current_note_start_tick;
    elapsed_ms += (current_note_elapsed_ticks * 1000) / configTICK_RATE_HZ;
    return elapsed_ms;
}
```

UI任务需要知道当前歌曲播放了多久，才能正确地绘制进度条。`calculateElapsedTime_ms`函数就负责这个计算。它通过后台任务更新的全局变量`shared_song_index`（当前歌曲）和`shared_note_index`（当前音符），首先计算出所有**已经播放完毕**的音符的总时长。然后，它再通过FreeRTOS的`xTaskGetTickCount()`函数，计算出**当前正在播放**的这个音符从开始到现在所经过的时间，并将其加到总时间上。通过这种方式，UI任务可以精确地计算出总的已播放时间，实现了与后台播放任务的进度同步。

## 四、效果展示与体验优化

最终的音乐模块提供了一个完整的、沉浸式的听觉和视觉体验。用户可以方便地选歌和控制播放。播放界面的信息布局合理，频谱动画为单调的蜂鸣器音乐增添了极大的趣味性。多任务架构的运用保证了在播放音乐的同时，UI依然保持流畅，可以随时响应用户的操作。

**[此处应有实际效果图，展示歌曲列表界面和带频谱的播放界面]**

## 五、总结与展望

音乐播放器模块是本项目中技术复杂度较高的一个，它综合运用了**多任务编程、PROGMEM内存优化、任务间通信和模拟数据可视化**等多种高级嵌入式技巧。它成功地将一个简单的蜂鸣器，变成了一个有趣的娱乐中心。尤其是通过模拟方式实现频谱可视化的思路，为在资源受限设备上实现酷炫效果提供了一个很好的范例。

未来的可扩展方向可以是：
-   **SD卡音乐播放**：增加SD卡槽和音频解码芯片（如VS1053），实现MP3等真实音频格式的播放。
-   **真实的频谱分析**：如果硬件性能允许，可以尝试使用一些为单片机优化的FFT库（如`arduinoFFT`），来实现基于麦克风输入的真实环境频谱分析。
-   **歌词同步显示**：如果能读取LRC等歌词文件，可以实现卡拉OK一样的歌词同步滚动功能。