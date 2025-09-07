# 简洁音乐模块：轻量级音乐播放

## 一、引言

简洁音乐模块 (`MusicMenuLite.cpp`) 是音乐模块 (`Buzzer.cpp`) 的一个轻量级版本，专注于提供核心的音乐播放功能，而省略了复杂的LED视觉化和频谱显示。它旨在为用户提供一个更简洁、资源占用更少的音乐播放选项，适用于对视觉效果要求不高或资源受限的场景。

## 二、实现思路与技术选型

### 2.1 核心播放逻辑复用

简洁音乐模块复用了 `Buzzer.h` 中定义的歌曲数据结构和歌曲列表，这意味着它可以使用与主音乐模块相同的歌曲库。这减少了代码重复，并保持了歌曲数据的一致性。

### 2.2 简化的FreeRTOS任务

模块创建了一个独立的FreeRTOS任务 `MusicLite_Playback_Task` 来处理音乐播放。与主音乐模块的 `Buzzer_Task` 类似，它负责音符的生成和播放模式的控制，但移除了与LED视觉化和频谱生成相关的代码，从而降低了任务的复杂性和资源消耗。

### 2.3 播放模式与控制

与主音乐模块相同，简洁音乐模块也支持多种播放模式（单曲循环、列表循环、随机播放）和暂停/恢复功能。用户可以通过旋转编码器切换播放模式，通过按键进行暂停/恢复操作。

### 2.4 简化的用户界面

模块提供了两个简化的UI界面：
- **歌曲列表界面**: 显示可播放的歌曲列表，与主音乐模块类似，用户可以通过旋转编码器选择歌曲。
- **播放界面**: 显示当前播放歌曲的名称、播放模式、实时时间、播放进度条和已播放/总时长。与主音乐模块不同，此界面不包含频谱视觉化，界面布局更简洁。

### 2.5 与其他模块的集成

- **实时时间显示**: 播放界面顶部实时时间显示依赖于 `weather.h` 中提供的 `timeinfo` 结构体和 `getLocalTime` 函数。
- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，音乐播放能够及时停止，避免功能冲突。

## 三、代码展示

### `MusicMenuLite.cpp`

```c++
#include "MusicMenuLite.h"
#include "Buzzer.h" // For song definitions, numSongs, PlayMode enum
#include "RotaryEncoder.h"
#include "Menu.h"
#include "weather.h" // for getLocalTime
#include "Alarm.h"   // for g_alarm_is_ringing
#include <freertos/task.h>

// --- Color definitions for progress bar ---
static const uint16_t song_colors[] = {
  TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_GREEN, TFT_ORANGE, TFT_PINK, TFT_VIOLET
};
static const int num_song_colors = sizeof(song_colors) / sizeof(song_colors[0]);

// --- Task and State Management ---
static TaskHandle_t musicLiteTaskHandle = NULL;
static volatile bool stopMusicLiteTask = false;
static volatile bool isPaused = false;

// --- Shared state between UI and Playback Task ---
static volatile int shared_song_index = 0;
static volatile int shared_note_index = 0;
static volatile int shared_total_notes = 0;
static volatile PlayMode shared_play_mode = LIST_LOOP;
static volatile TickType_t current_note_start_tick = 0; // Time when the current note started playing
static volatile int shared_current_note_duration = 0;
static volatile int shared_current_note_frequency = 0;

// --- Forward Declarations ---
static void MusicLite_Playback_Task(void *pvParameters);
static void displaySongList_Lite(int selectedIndex, int displayOffset);
static void displayPlayingScreen_Lite(uint16_t progress_bar_color);
static void stop_lite_playback();
static uint32_t calculateSongDuration_ms(int songIndex);
static uint32_t calculateElapsedTime_ms();

// --- Helper Functions ---
static uint32_t calculateSongDuration_ms(int songIndex) {
    if (songIndex < 0 || songIndex >= numSongs) return 0;
    Song song;
    memcpy_P(&song, &songs[songIndex], sizeof(Song));
    uint32_t total_ms = 0;
    for (int i = 0; i < song.length; i++) {
        total_ms += pgm_read_word(song.durations + i);
    }
    return total_ms;
}

static uint32_t calculateElapsedTime_ms() {
    if (shared_song_index < 0 || shared_song_index >= numSongs) return 0;
    Song song;
    memcpy_P(&song, &songs[shared_song_index], sizeof(Song));
    uint32_t elapsed_ms = 0;
    // Sum of durations of already played notes
    for (int i = 0; i < shared_note_index; i++) {
        elapsed_ms += pgm_read_word(song.durations + i);
    }
    // Add time elapsed in the current note
    TickType_t current_note_elapsed_ticks = xTaskGetTickCount() - current_note_start_tick;
    elapsed_ms += (current_note_elapsed_ticks * 1000) / configTICK_RATE_HZ;
    return elapsed_ms;
}


// --- Playback Task (Alarm.cpp style) ---
void MusicLite_Playback_Task(void *pvParameters) {
    int songIndex = *(int*)pvParameters;

    for (;;) { // Infinite loop to handle song changes
        shared_song_index = songIndex;
        Song song;
        memcpy_P(&song, &songs[songIndex], sizeof(Song));
        shared_total_notes = song.length;
        shared_note_index = 0;

        for (int i = 0; i < song.length; i++) {
            if (stopMusicLiteTask) {
                noTone(BUZZER_PIN);
                vTaskDelete(NULL);
            }

            while (isPaused) {
                if (stopMusicLiteTask) {
                    noTone(BUZZER_PIN);
                    vTaskDelete(NULL);
                }
                noTone(BUZZER_PIN);
                shared_current_note_duration = 0;
                shared_current_note_frequency = 0;
                current_note_start_tick = xTaskGetTickCount(); // Reset start tick while paused
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            shared_note_index = i;
            current_note_start_tick = xTaskGetTickCount();
            int note = pgm_read_word(song.melody + i);
            int duration = pgm_read_word(song.durations + i);

            shared_current_note_duration = duration;
            shared_current_note_frequency = note;

            if (note > 0) {
                tone(BUZZER_PIN, note, duration * 0.9);
            }
            
            vTaskDelay(pdMS_TO_TICKS(duration));
        }
        
        shared_current_note_duration = 0;
        shared_current_note_frequency = 0;

        // Pause for 2 seconds before playing the next song
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Song finished, select next one based on play mode
        if (shared_play_mode == SINGLE_LOOP) {
            // Just repeat the same song index
        } else if (shared_play_mode == LIST_LOOP) {
            songIndex = (songIndex + 1) % numSongs;
        } else if (shared_play_mode == RANDOM_PLAY) {
            if (numSongs > 1) {
                int currentSong = songIndex;
                do {
                    songIndex = random(numSongs);
                } while (songIndex == currentSong);
            }
        }
    }
}

void stop_lite_playback() {
    if (musicLiteTaskHandle != NULL) {
        vTaskDelete(musicLiteTaskHandle);
        musicLiteTaskHandle = NULL;
    }
    noTone(BUZZER_PIN);
    isPaused = false;
    stopMusicLiteTask = false; // Reset flag for cleanliness
}

// --- UI Drawing Functions ---
void displaySongList_Lite(int selectedIndex, int displayOffset) {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  menuSprite.setTextSize(2);
  menuSprite.setTextDatum(MC_DATUM);
  menuSprite.drawString("Music Lite", 120, 28);
  
  const int visibleSongs = 3;
  for (int i = 0; i < visibleSongs; i++) {
    int songIdx = displayOffset + i;
    if (songIdx >= numSongs) break;
   
    int yPos = 60 + i * 50;
    
    if (songIdx == selectedIndex) {
      menuSprite.fillRoundRect(10, yPos - 18, 220, 36, 5, 0x001F); 
      menuSprite.setTextSize(2);
      menuSprite.setTextColor(TFT_WHITE, 0x001F);
      menuSprite.setTextDatum(MC_DATUM);
      menuSprite.drawString(songs[songIdx].name, 120, yPos);
    } else {
      menuSprite.fillRoundRect(10, yPos - 18, 220, 36, 5, TFT_BLACK);
      menuSprite.setTextSize(1);
      menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
      menuSprite.setTextDatum(MC_DATUM);
      menuSprite.drawString(songs[songIdx].name, 120, yPos);
    }
  }
  
  menuSprite.setTextDatum(TL_DATUM);
  menuSprite.pushSprite(0, 0);
}

void displayPlayingScreen_Lite(uint16_t progress_bar_color) {
    uint32_t elapsed_ms = calculateElapsedTime_ms();
    uint32_t total_ms = calculateSongDuration_ms(shared_song_index);

    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

    // Song Name
    menuSprite.setTextSize(2);
    menuSprite.drawString(songs[shared_song_index].name, 120, 20);

    // Note Info
    char noteInfoStr[30];
    snprintf(noteInfoStr, sizeof(noteInfoStr), "%d Hz  %d ms", shared_current_note_frequency, shared_current_note_duration);
    menuSprite.setTextSize(2);
    menuSprite.drawString(noteInfoStr, 120, 50);

    // Play Mode
    switch (shared_play_mode) {
        case SINGLE_LOOP: menuSprite.drawString("Single Loop", 120, 80); break;
        case LIST_LOOP: menuSprite.drawString("List Loop", 120, 80); break;
        case RANDOM_PLAY: menuSprite.drawString("Random", 120, 80); break;
    }

    // System Time
    extern struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        menuSprite.setTextSize(2);
        menuSprite.drawString(timeStr, 120, 110);
    }

    // Play/Pause Indicator
    if (isPaused) {
        menuSprite.drawString("Paused", 120, 140);
    } else {
        menuSprite.drawString("Playing", 120, 140);
    }

    // Progress Bar
    int progressWidth = 0;
    if (total_ms > 0) {
        progressWidth = (elapsed_ms * 220) / total_ms;
        if (progressWidth > 220) progressWidth = 220;
    }
    menuSprite.drawRoundRect(10, 165, 220, 10, 5, progress_bar_color);
    menuSprite.fillRoundRect(10, 165, progressWidth, 10, 5, progress_bar_color);

    // Time Display
    char time_buf[20];
    int elapsed_sec = elapsed_ms / 1000;
    int total_sec = total_ms / 1000;
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d / %02d:%02d", 
             elapsed_sec / 60, elapsed_sec % 60,
             total_sec / 60, total_sec % 60);
    menuSprite.setTextSize(2);
    menuSprite.drawString(time_buf, 120, 190);

    // Note Count Display
    char note_count_buf[20];
    snprintf(note_count_buf, sizeof(note_count_buf), "%d / %d",
             shared_note_index + 1, shared_total_notes); // +1 because shared_note_index is 0-based
    menuSprite.setTextSize(2);
    menuSprite.drawString(note_count_buf, 120, 210);

    menuSprite.pushSprite(0, 0);
}

// --- Main Menu Function ---
void MusicMenuLite() {
    int selectedSongIndex = 0;
    int displayOffset = 0;
    const int visibleSongs = 3;

    // --- Song Selection Loop ---
    displaySongList_Lite(selectedSongIndex, displayOffset);
    while(true) {
        if (g_alarm_is_ringing) return;

        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            selectedSongIndex = (selectedSongIndex + encoderChange + numSongs) % numSongs;
            if (selectedSongIndex < displayOffset) {
                displayOffset = selectedSongIndex;
            } else if (selectedSongIndex >= displayOffset + visibleSongs) {
                displayOffset = selectedSongIndex - visibleSongs + 1;
            }
            displaySongList_Lite(selectedSongIndex, displayOffset);
            tone(BUZZER_PIN, 1000, 50);
        }
        
        if (readButton()) {
            tone(BUZZER_PIN, 1500, 50);
            break; // Song selected, exit loop
        }

        if (readButtonLongPress()) {
            return; // Exit menu
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // --- Playback UI Loop ---
    stopMusicLiteTask = false;
    isPaused = false;
    shared_play_mode = LIST_LOOP; // Default to list loop
    xTaskCreatePinnedToCore(MusicLite_Playback_Task, "MusicLite_Playback_Task", 4096, &selectedSongIndex, 2, &musicLiteTaskHandle, 0);

    unsigned long lastScreenUpdateTime = 0;

    while(true) {
        if (g_alarm_is_ringing) {
            stop_lite_playback();
            return;
        }

        // Handle Input
        if (readButtonLongPress()) {
            stop_lite_playback();
            return;
        }

        if (readButton()) {
            isPaused = !isPaused;n            tone(BUZZER_PIN, 1000, 50);
            lastScreenUpdateTime = 0; // Force screen update
        }

        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            int mode = (int)shared_play_mode;
            mode = (mode + encoderChange + 3) % 3; // Add 3 to handle negative results
            shared_play_mode = (PlayMode)mode;
            tone(BUZZER_PIN, 1200, 50);
            lastScreenUpdateTime = 0; // Force screen update
        }

        // Update screen periodically
        if (millis() - lastScreenUpdateTime > 200) { // Update 5 times a second
            uint16_t current_color = song_colors[shared_song_index % num_song_colors];
            displayPlayingScreen_Lite(current_color);
            lastScreenUpdateTime = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

## 四、代码解读

### 4.1 播放状态与共享变量

```c++
// --- Task and State Management ---
static TaskHandle_t musicLiteTaskHandle = NULL;
static volatile bool stopMusicLiteTask = false;
static volatile bool isPaused = false;

// --- Shared state between UI and Playback Task ---
static volatile int shared_song_index = 0;
static volatile int shared_note_index = 0;
static volatile int shared_total_notes = 0;
static volatile PlayMode shared_play_mode = LIST_LOOP;
static volatile TickType_t current_note_start_tick = 0; // Time when the current note started playing
static volatile int shared_current_note_duration = 0;
static volatile int shared_current_note_frequency = 0;
```
- `musicLiteTaskHandle`: FreeRTOS任务句柄，用于管理音乐播放任务。
- `stopMusicLiteTask`, `isPaused`: 控制音乐任务的停止和暂停状态的标志。
- `shared_song_index`, `shared_note_index`, `shared_total_notes`, `shared_play_mode`, `current_note_start_tick`, `shared_current_note_duration`, `shared_current_note_frequency`: 任务和UI之间共享的变量，用于跟踪当前播放的歌曲、音符、总音符数、播放模式、音符开始时间、当前音符时长和频率，以便UI能够实时显示播放信息。

### 4.2 音乐播放任务 `MusicLite_Playback_Task()`

```c++
void MusicLite_Playback_Task(void *pvParameters) {
    int songIndex = *(int*)pvParameters;

    for (;;) { // Infinite loop to handle song changes
        shared_song_index = songIndex;
        Song song;
        memcpy_P(&song, &songs[songIndex], sizeof(Song));
        shared_total_notes = song.length;
        shared_note_index = 0;

        for (int i = 0; i < song.length; i++) {
            if (stopMusicLiteTask) { noTone(BUZZER_PIN); vTaskDelete(NULL); }
            while (isPaused) { /* ... 暂停逻辑 ... */ }

            shared_note_index = i;
            current_note_start_tick = xTaskGetTickCount();
            int note = pgm_read_word(song.melody + i);
            int duration = pgm_read_word(song.durations + i);

            shared_current_note_duration = duration;
            shared_current_note_frequency = note;

            if (note > 0) { tone(BUZZER_PIN, note, duration * 0.9); }
            vTaskDelay(pdMS_TO_TICKS(duration));
        }
        
        shared_current_note_duration = 0;
        shared_current_note_frequency = 0;

        vTaskDelay(pdMS_TO_TICKS(2000)); // Pause for 2 seconds before playing the next song

        // Song finished, select next one based on play mode
        if (shared_play_mode == SINGLE_LOOP) { /* ... */ }
        else if (shared_play_mode == LIST_LOOP) { songIndex = (songIndex + 1) % numSongs; }
        else if (shared_play_mode == RANDOM_PLAY) { /* ... */ }
    }
}
```
- 这是一个FreeRTOS任务，负责实际的音乐播放。
- 它会根据 `songIndex` 播放当前歌曲的每个音符，并更新 `shared_song_index`、`shared_note_index`、`shared_total_notes` 等共享变量，以便UI能够获取最新播放信息。
- 在播放每个音符前，会检查 `stopMusicLiteTask` 和 `isPaused` 标志，以响应停止或暂停请求。
- 歌曲播放结束后，会暂停2秒，然后根据 `shared_play_mode` 决定下一首播放的歌曲。

### 4.3 UI 绘制 `displaySongList_Lite()` 与 `displayPlayingScreen_Lite()`

```c++
void displaySongList_Lite(int selectedIndex, int displayOffset) { /* ... */ }
void displayPlayingScreen_Lite(uint16_t progress_bar_color) { /* ... */ }
```
- `displaySongList_Lite()`: 绘制歌曲选择列表界面，高亮显示当前选中歌曲。与主音乐模块的列表界面类似。
- `displayPlayingScreen_Lite()`: 绘制音乐播放界面，显示歌曲名称、当前音符信息（频率和时长）、播放模式、系统时间、播放/暂停状态、播放进度条、已播放/总时长和音符计数。此界面相比主音乐模块的播放界面，移除了频谱视觉化，更加简洁。
- 两个函数都使用 `menuSprite` 进行双缓冲绘制。

### 4.4 主简洁音乐菜单函数 `MusicMenuLite()`

```c++
void MusicMenuLite() {
    int selectedSongIndex = 0;
    int displayOffset = 0;
    const int visibleSongs = 3;

    // --- Song Selection Loop ---
    displaySongList_Lite(selectedSongIndex, displayOffset);
    while(true) { /* ... 歌曲选择逻辑 ... */ }

    // --- Playback UI Loop ---
    stopMusicLiteTask = false;
    isPaused = false;
    shared_play_mode = LIST_LOOP; // Default to list loop
    xTaskCreatePinnedToCore(MusicLite_Playback_Task, "MusicLite_Playback_Task", 4096, &selectedSongIndex, 2, &musicLiteTaskHandle, 0);

    unsigned long lastScreenUpdateTime = 0;

    while(true) { /* ... 播放控制逻辑 ... */ }
}
```
- `MusicMenuLite()` 是简洁音乐模块的主入口函数，它包含两个主要阶段：
    1.  **歌曲选择阶段**: 用户通过旋转编码器选择歌曲，单击按键确认选择。在此阶段，`displaySongList_Lite()` 负责UI绘制。
    2.  **音乐播放阶段**: 选定歌曲后，创建 `MusicLite_Playback_Task` 任务开始播放音乐。用户可以通过按键暂停/恢复，旋转编码器切换播放模式。在此阶段，`displayPlayingScreen_Lite()` 负责UI绘制。
- **退出逻辑**: 
    - 长按按键或闹钟响铃 (`g_alarm_is_ringing`) 会停止音乐任务，并退出 `MusicMenuLite`。
    - `stop_lite_playback()` 函数负责清理任务和资源。

## 五、总结与展望

简洁音乐模块提供了一个轻量级、资源友好的音乐播放解决方案，它在功能上专注于核心播放，并通过复用现有歌曲数据和简化UI，实现了代码的精简和高效。这使得它成为对设备资源要求较低的理想选择。

未来的改进方向：
1.  **更精简的UI**: 进一步简化播放界面，只显示最核心的信息。
2.  **音效库扩展**: 增加更多简单的音效或旋律，丰富播放内容。
3.  **低功耗模式**: 在播放音乐时，优化屏幕刷新频率和CPU使用，以降低功耗。
4.  **自定义播放列表**: 允许用户创建和管理简单的播放列表。

谢谢大家
