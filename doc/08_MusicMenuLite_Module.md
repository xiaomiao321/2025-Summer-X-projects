# 「轻装上阵」一个极简风格的音乐播放器

## 一、模块概述

在实现了功能丰富的全功能音乐播放器之后，我们又打造了`MusicMenuLite`——一个追求极致简洁、注重核心体验的“轻量版”播放器。它的存在是为了满足一种不同的用户场景：用户只想快速选歌、听歌，不希望被复杂的视觉元素（如频谱）所打扰，同时希望UI能提供更纯粹、更直接的信息反馈。

`MusicMenuLite`的核心功能与标准版音乐播放器一致，支持歌曲选择、播放/暂停、多种播放模式切换。但它的界面风格完全不同，移除了频谱动画，采用了更经典、更注重信息呈现的文本布局，将歌曲名、播放模式、系统时间、进度条和数字时间等核心信息清晰地展示给用户。

## 二、实现思路与技术选型

### 设计哲学：从“炫技”到“实用”

`MusicMenuLite`与`Buzzer`模块最大的不同在于设计哲学的转变。如果说`Buzzer`模块追求的是“声色俱佳”的感官刺激，那么`MusicMenuLite`追求的就是“清晰高效”的信息传达。这种差异化设计满足了不同用户的审美和使用偏好。

技术选型上，`MusicMenuLite`与`Buzzer`模块一脉相承，同样基于**FreeRTOS多任务**来分离播放逻辑和UI刷新，同样使用**PROGMEM**来存储音乐数据。它们共享来自`Buzzer.h`的歌曲数据定义，这体现了良好的代码复用。其核心差异主要体现在UI的实现和任务间通信的细节上。

### 任务间通信：更丰富的共享状态

为了在纯文本UI上显示更详尽的播放信息，`MusicMenuLite`的UI任务需要从后台播放任务那里获取更多的数据。因此，我们扩展了共享变量的集合：

-   `shared_current_note_duration` (volatile int): 当前播放音符的总时长。
-   `shared_current_note_frequency` (volatile int): 当前播放音符的频率。

后台任务在播放每个音符前，都会更新这两个变量。前台UI任务则定时读取它们，并直接显示在屏幕上。这种方式让用户能实时看到播放器内部的工作细节（“正在播放XXX赫兹的音符，持续XXX毫秒”），带来了一种独特的“极客”或“调试”风格的体验。

## 三、代码深度解读

### 后台播放任务的扩展 (`MusicMenuLite.cpp`)

```cpp
void MusicLite_Playback_Task(void *pvParameters) {
    int songIndex = *(int*)pvParameters;

    for (;;) { // 歌曲切换循环
        // ...
        for (int i = 0; i < song.length; i++) { // 单曲播放循环
            // ... 暂停和退出逻辑 ...

            int note = pgm_read_word(song.melody + i);
            int duration = pgm_read_word(song.durations + i);

            // 更新共享变量，供UI任务读取
            shared_current_note_duration = duration;
            shared_current_note_frequency = note;

            if (note > 0) {
                tone(BUZZER_PIN, note, duration * 0.9);
            }
            
            vTaskDelay(pdMS_TO_TICKS(duration));
        }
        
        // 播放结束，清零状态
        shared_current_note_duration = 0;
        shared_current_note_frequency = 0;

        // ... 切换到下一首歌 ...
    }
}
```

对比`Buzzer`模块的后台任务，这个“轻量版”任务的核心区别在于增加了对`shared_current_note_duration`和`shared_current_note_frequency`这两个全局变量的写操作。它在每次调用`tone()`函数之前，都将音符的频率和时长信息“广播”出去。当一首歌播放完毕，它还会主动将这两个变量清零。这种精细的状态同步，是实现信息驱动型UI的关键。

### 信息驱动的UI绘制 (`MusicMenuLite.cpp`)

```cpp
void displayPlayingScreen_Lite(uint16_t progress_bar_color) {
    // ...
    menuSprite.fillScreen(TFT_BLACK);
    // ...

    // 显示从后台任务获取的音符信息
    char noteInfoStr[30];
    snprintf(noteInfoStr, sizeof(noteInfoStr), "%d Hz  %d ms", shared_current_note_frequency, shared_current_note_duration);
    menuSprite.setTextSize(2);
    menuSprite.drawString(noteInfoStr, 120, 50);

    // ... 显示歌曲名、播放模式、系统时间等 ...

    // 绘制进度条和时间
    uint32_t elapsed_ms = calculateElapsedTime_ms();
    uint32_t total_ms = calculateSongDuration_ms(shared_song_index);
    // ...

    menuSprite.pushSprite(0, 0);
}
```

这是`MusicMenuLite`的播放界面绘制函数。它与`Buzzer`模块的对应函数有显著不同：
1.  **无频谱**：它完全没有调用任何频谱计算或绘制的逻辑。
2.  **信息密度更高**：它从`shared_current_note_frequency`和`shared_current_note_duration`等共享变量中读取信息，并使用`snprintf`格式化成字符串，直接展示在屏幕上。
3.  **布局清晰**：整个界面被规划为几个清晰的文本区域，分别用于显示歌名、音符数据、播放模式、系统时间、播放/暂停状态和进度信息，是一种典型的仪表盘式（Dashboard）布局。

这种UI的实现方式，虽然视觉冲击力不如频谱动画，但其信息直观性更强，且性能开销更低。

## 四、效果展示与体验优化

`MusicMenuLite`提供了一个与标准版截然不同的交互体验。它的界面干净、响应迅速，所有关键信息一览无余。对于那些更关心播放内容和状态，而非视觉效果的用户来说，这是一个更高效、更“纯粹”的音乐播放器。不同颜色的进度条也为单调的文本界面增添了一抹亮色。

**[此处应有实际效果图，展示MusicMenuLite的歌曲列表和播放界面]**

## 五、总结与展望

`MusicMenuLite`模块的开发，展示了在同一套核心逻辑（后台播放任务）和数据（歌曲库）的基础上，通过改变UI设计和任务间通信的细节，可以衍生出满足不同用户需求的、风格迥异的功能模块。这是一个关于“关注点分离”（Separation of Concerns）和“代码复用”的优秀实践。

它证明了好的底层架构设计，能让上层应用的开发变得灵活而高效。`Buzzer`和`MusicMenuLite`就像是同一款汽车换上了不同的仪表盘和内饰，核心动力系统并未改变，但驾驶体验却截然不同。

未来的可扩展方向可以与`Buzzer`模块合并，例如：
-   **统一入口**：将两者合并为一个“音乐”入口，在进入后再让用户选择“炫彩模式”或“简洁模式”。
-   **动态切换**：在播放界面增加一个“切换视图”的按钮，允许用户在频谱界面和信息界面之间随时切换，以满足即时的需求变化。