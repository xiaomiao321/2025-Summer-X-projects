# 音乐模块：蜂鸣器音乐播放与LED视觉化

## 一、引言

音乐模块为多功能时钟增添了娱乐功能，它能够通过蜂鸣器播放预设的音乐，并结合LED灯带提供动态的视觉效果。本模块旨在提供一个可选择歌曲、控制播放模式（单曲循环、列表循环、随机播放）和暂停/恢复的音乐播放器，同时通过LED灯带的彩虹效果增强用户体验。

## 二、实现思路与技术选型

### 2.1 歌曲数据结构与PROGMEM存储

每首歌曲的数据都封装在 `Song` 结构体中，包含歌曲名称、音高数组 (`melody`)、音符持续时间数组 (`durations`)、歌曲长度和分类。为了节省宝贵的RAM空间，所有歌曲数据都存储在Flash（PROGMEM）中，通过 `memcpy_P` 和 `pgm_read_word` 等函数在运行时读取。

### 2.2 FreeRTOS多任务并发

为了实现音乐播放和LED视觉化的并行处理，模块利用FreeRTOS创建了两个独立的任务：
- `Buzzer_Task`: 负责蜂鸣器音乐的播放逻辑，包括音符的生成和播放模式的控制。
- `Led_Rainbow_Task`: 负责LED灯带的彩虹效果生成和更新。

这种多任务设计确保了音乐播放和LED效果能够流畅地同时运行，互不干扰。

### 2.3 播放模式与控制

模块支持多种播放模式，通过 `PlayMode` 枚举进行管理：
- `SINGLE_LOOP`: 单曲循环。
- `LIST_LOOP`: 列表循环，按顺序播放下一首。
- `RANDOM_PLAY`: 随机播放，从列表中随机选择下一首。

用户可以通过旋转编码器切换播放模式，通过按键进行暂停/恢复操作。

### 2.4 伪频谱视觉化

为了在播放界面提供更丰富的视觉反馈，模块实现了一个简单的伪频谱视觉化效果。`generate_fake_spectrum` 函数根据当前播放音符的频率生成一个模拟的频谱数据，并在UI上以柱状图的形式显示。虽然不是真实的FFT频谱，但能提供动态的视觉效果。

### 2.5 用户界面与交互

模块提供了两个主要的UI界面：
- **歌曲列表界面**: 显示所有可播放的歌曲列表，用户可以通过旋转编码器选择歌曲。
- **播放界面**: 显示当前播放歌曲的名称、播放模式、实时时间、伪频谱、播放进度条和已播放/总时长。用户可以在此界面暂停/恢复播放，或切换播放模式。

### 2.6 与其他模块的集成

- **实时时间显示**: 播放界面顶部实时时间显示依赖于 `weather.h` 中提供的 `timeinfo` 结构体和 `getLocalTime` 函数。
- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，音乐播放能够及时停止，避免功能冲突。

## 三、代码展示

### `Buzzer.cpp`

```c++
#include "Buzzer.h"
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include <time.h>
#include "LED.h"
#include <math.h>
#include "Menu.h"
#include "MQTT.h"
#include "weather.h"
#include "Alarm.h"
#include <freertos/task.h>

// --- Task Handles ---
TaskHandle_t buzzerTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;

// --- Playback State ---
PlayMode currentPlayMode = LIST_LOOP;
volatile bool stopBuzzerTask = false;
volatile bool isPaused = false;
volatile bool stopLedTask = false;

// --- Shared state for UI ---
static volatile int shared_song_index = 0;
static volatile int shared_note_index = 0;
static volatile TickType_t current_note_start_tick = 0;

// --- Spectrum Visualization ---
#define NUM_BANDS 16
volatile float spectrum[NUM_BANDS];

// --- Song Data ---
const Song songs[] PROGMEM= {
  { "Cai Bu Tou", melody_cai_bu_tou, durations_cai_bu_tou, sizeof(melody_cai_bu_tou) / sizeof(melody_cai_bu_tou[0]), 0 },
  {"Chun Jiao Yu Zhi Ming",melody_chun_jiao_yu_zhi_ming,durations_chun_jiao_yu_zhi_ming,sizeof(melody_chun_jiao_yu_zhi_ming)/sizeof(melody_chun_jiao_yu_zhi_ming[0]),1},
  { "Cheng Du", melody_cheng_du, durations_cheng_du, sizeof(melody_cheng_du)/sizeof(melody_cheng_du[0]), 2 },
  {"Hai Kuo Tian Kong",melody_hai_kuo_tian_kong,durations_hai_kuo_tian_kong, sizeof(melody_hai_kuo_tian_kong)/sizeof(melody_hai_kuo_tian_kong[0]), 3 },
  { "Hong Dou", melody_hong_dou, durations_hong_dou, sizeof(melody_hong_dou)/sizeof(melody_hong_dou[0]), 4 },
  { "Hou Lai", melody_hou_lai, durations_hou_lai, sizeof(melody_hou_lai)/sizeof(melody_hou_lai[0]), 0 },
  { "Kai Shi Dong Le", melody_kai_shi_dong_le, durations_kai_shi_dong_le, sizeof(melody_kai_shi_dong_le)/sizeof(melody_kai_shi_dong_le[0]), 1 },
  { "Lv Se", melody_lv_se, durations_lv_se, sizeof(melody_lv_se)/sizeof(melody_lv_se[0]), 2 },
  {"Mi Ren De Wei Xian",melody_mi_ren_de_wei_xian,durations_mi_ren_de_wei_xian,sizeof(melody_mi_ren_de_wei_xian)/sizeof(melody_mi_ren_de_wei_xian[0]),3},
  { "Qing Hua Ci", melody_qing_hua_ci, durations_qing_hua_ci, sizeof(melody_qing_hua_ci)/sizeof(melody_qing_hua_ci[0]), 3 },
  { "Xin Qiang", melody_xin_qiang, durations_xin_qiang,sizeof(melody_xin_qiang)/sizeof(melody_xin_qiang[0]), 4 },
  { "You Dian Tian", melody_you_dian_tian, durations_you_dian_tian, sizeof(melody_you_dian_tian)/sizeof(melody_you_dian_tian[0]), 0 },
  {"Windows XP",melody_windows,durations_windows,sizeof(melody_windows) / sizeof(melody_windows[0]),3},
};
const int numSongs = sizeof(songs) / sizeof(songs[0]);

// --- UI State ---
int selectedSongIndex = 0;
int displayOffset = 0;
const int visibleSongs = 3;

// --- Forward Declarations ---
static void stop_buzzer_playback();

// --- Helper Functions ---
void generate_fake_spectrum(int frequency) {
    for (int i = 0; i < NUM_BANDS; i++) { spectrum[i] = 0.0f; }
    if (frequency <= 0) return;
    const float min_log_freq = log(200);
    const float max_log_freq = log(2000);
    float log_freq = log(frequency);
    int band = NUM_BANDS * (log_freq - min_log_freq) / (max_log_freq - min_log_freq);
    if (band < 0) band = 0;
    if (band >= NUM_BANDS) band = NUM_BANDS - 1;
    spectrum[band] = 70.0f;
    if (band > 0) spectrum[band - 1] = 35.0f;
    if (band < NUM_BANDS - 1) spectrum[band + 1] = 35.0f;
}

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
    for (int i = 0; i < shared_note_index; i++) {
        elapsed_ms += pgm_read_word(song.durations + i);
    }
    TickType_t current_note_elapsed_ticks = xTaskGetTickCount() - current_note_start_tick;
    elapsed_ms += (current_note_elapsed_ticks * 1000) / configTICK_RATE_HZ;
    return elapsed_ms;
}

// --- UI Drawing ---
void displaySongList(int selectedIndex) {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  menuSprite.setTextSize(2);
  menuSprite.setTextDatum(MC_DATUM);
  menuSprite.drawString("Music Menu", 120, 28);
  for (int i = 0; i < visibleSongs; i++) {
    int songIdx = displayOffset + i;
    if (songIdx >= numSongs) break;
    int yPos = 60 + i * 50;
    if (songIdx == selectedIndex) {
      menuSprite.fillRoundRect(10, yPos - 18, 220, 36, 5, 0x001F);
      menuSprite.setTextSize(2);
      menuSprite.setTextColor(TFT_WHITE, 0x001F);
      menuSprite.drawString(songs[songIdx].name, 120, yPos);
    } else {
      menuSprite.fillRoundRect(10, yPos - 18, 220, 36, 5, TFT_BLACK);
      menuSprite.setTextSize(1);
      menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
      menuSprite.drawString(songs[songIdx].name, 120, yPos);
    }
  }
  menuSprite.setTextDatum(TL_DATUM);
  menuSprite.pushSprite(0, 0);
}

void displayPlayingSong() {
    uint32_t elapsed_ms = calculateElapsedTime_ms();
    uint32_t total_ms = calculateSongDuration_ms(shared_song_index);

    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

    menuSprite.setTextSize(2);
    menuSprite.drawString(songs[shared_song_index].name, 120, 20);

    extern struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        menuSprite.setTextSize(2);
        menuSprite.drawString(timeStr, 120, 50);
    }

    String status_text = isPaused ? "Paused / " : "Playing / ";
    switch (currentPlayMode) {
        case SINGLE_LOOP: status_text += "Single Loop"; break;
        case LIST_LOOP: status_text += "List Loop"; break;
        case RANDOM_PLAY: status_text += "Random"; break;
    }
    menuSprite.drawString(status_text, 120, 80);

    int barWidth = 8;
    int barSpacing = 14;
    for (int i = 0; i < NUM_BANDS; i++) {
        int barHeight = spectrum[i];
        if (barHeight > 70) barHeight = 70;
        if (barHeight > 0) {
            menuSprite.fillRoundRect(10 + i * barSpacing, 170 - barHeight, barWidth, barHeight, 4, TFT_CYAN);
        }
    }

    int progressWidth = 0;
    if (total_ms > 0) {
        progressWidth = (elapsed_ms * 220) / total_ms;
        if (progressWidth > 220) progressWidth = 220;
    }
    menuSprite.drawRoundRect(10, 185, 220, 10, 5, TFT_WHITE);
    menuSprite.fillRoundRect(10, 185, progressWidth, 10, 5, TFT_WHITE);

    char time_buf[20];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d / %02d:%02d", elapsed_ms / 60000, (elapsed_ms / 1000) % 60, total_ms / 60000, (total_ms / 1000) % 60);
    menuSprite.drawString(time_buf, 120, 210);

    menuSprite.pushSprite(0, 0);
}

// --- Tasks ---
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if(WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void Led_Rainbow_Task(void *pvParameters) {
    for (uint16_t j = 0; ; j++) {
        if (stopLedTask) {
            strip.clear();
            strip.show();
            vTaskDelete(NULL);
        }
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void Buzzer_Task(void *pvParameters) {
  int songIdx = *(int*)pvParameters;
  for(;;) {
    shared_song_index = songIdx;
    Song song;
    memcpy_P(&song, &songs[songIdx], sizeof(Song));
    for (int i = 0; i < song.length; i++) {
      shared_note_index = i;
      current_note_start_tick = xTaskGetTickCount();
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        vTaskDelete(NULL);
      }
      while (isPaused) {
        generate_fake_spectrum(0);
        noTone(BUZZER_PIN);
        current_note_start_tick = xTaskGetTickCount();
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      int note = pgm_read_word(song.melody+i);
      int duration = pgm_read_word(song.durations+i);
      generate_fake_spectrum(note);
      if (note > 0) {
        tone(BUZZER_PIN, note, duration*0.9);
      }
      vTaskDelay(pdMS_TO_TICKS(duration));
    }
    if (currentPlayMode == SINGLE_LOOP) {}
    else if (currentPlayMode == LIST_LOOP) {
      songIdx = (songIdx + 1) % numSongs;
    } else if (currentPlayMode == RANDOM_PLAY) {
      if (numSongs > 1) {
        int currentSong = songIdx;
        do { songIdx = random(numSongs); } while (songIdx == currentSong);
      }
    }
  }
}

// This is the restored task for background music (e.g., boot, chime)
void Buzzer_PlayMusic_Task(void *pvParameters) {
  int songIndex = *(int*)pvParameters;
  Song song;
  memcpy_P(&song, &songs[songIndex], sizeof(Song));

  for (int i = 0; i < song.length; i++) {
    if (stopBuzzerTask) { // Check the global stop flag
      noTone(BUZZER_PIN);
      vTaskDelete(NULL);
      return;
    }

    int note = pgm_read_word(song.melody + i);
    int duration = pgm_read_word(song.durations + i);
    
    if (note > 0) {
        tone(BUZZER_PIN, note, duration * 0.9);
    }
    
    vTaskDelay(pdMS_TO_TICKS(duration));
  }
  
  vTaskDelete(NULL); // Self-delete when done
}

// --- Main Menu Function ---
void Buzzer_Init() { pinMode(BUZZER_PIN, OUTPUT); }

static void stop_buzzer_playback() {
    if (buzzerTaskHandle != NULL) { vTaskDelete(buzzerTaskHandle); buzzerTaskHandle = NULL; }
    if (ledTaskHandle != NULL) { vTaskDelete(ledTaskHandle); ledTaskHandle = NULL; }
    noTone(BUZZER_PIN);
    strip.clear();
    strip.show();
    isPaused = false;
    stopBuzzerTask = false;
    stopLedTask = false;
}

void BuzzerMenu() {
  selectedSongIndex = 0;
  displayOffset = 0;
  isPaused = false;
  displaySongList(selectedSongIndex);
  while (1) {
    if (exitSubMenu || g_alarm_is_ringing) { return; }
    int encoderChange = readEncoder();
    if (encoderChange != 0) {
      selectedSongIndex = (selectedSongIndex + encoderChange + numSongs) % numSongs;
      if (selectedSongIndex < displayOffset) { displayOffset = selectedSongIndex; }
      else if (selectedSongIndex >= displayOffset + visibleSongs) { displayOffset = selectedSongIndex - visibleSongs + 1; }
      displaySongList(selectedSongIndex);
      tone(BUZZER_PIN, 1000, 50);
    }
    if (readButton()) { tone(BUZZER_PIN, 1500, 50); break; }
    if (readButtonLongPress()) { return; }
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  stopBuzzerTask = false;
  stopLedTask = false;
  isPaused = false;
  currentPlayMode = LIST_LOOP;
  xTaskCreatePinnedToCore(Buzzer_Task, "Buzzer_Task", 4096, &selectedSongIndex, 2, &buzzerTaskHandle, 0);
  xTaskCreatePinnedToCore(Led_Rainbow_Task, "Led_Rainbow_Task", 2048, NULL, 1, &ledTaskHandle, 0);

  unsigned long lastScreenUpdateTime = 0;
  while (1) {
    if (exitSubMenu || g_alarm_is_ringing) { stop_buzzer_playback(); return; }
    if (readButtonLongPress()) { stop_buzzer_playback(); return; }
    if (readButton()) { isPaused = !isPaused; tone(BUZZER_PIN, 1000, 50); }
    int encoderChange = readEncoder();
    if (encoderChange != 0) {
      int mode = (int)currentPlayMode;
      mode = (mode + encoderChange + 3) % 3;
      currentPlayMode = (PlayMode)mode;
    }
    if (millis() - lastScreenUpdateTime > 100) {
        displayPlayingSong();
        lastScreenUpdateTime = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
```

## 四、代码解读

### 4.1 歌曲数据与播放状态

```c++
// --- Task Handles ---
TaskHandle_t buzzerTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;

// --- Playback State ---
PlayMode currentPlayMode = LIST_LOOP;
volatile bool stopBuzzerTask = false;
volatile bool isPaused = false;
volatile bool stopLedTask = false;

// --- Shared state for UI ---
static volatile int shared_song_index = 0;
static volatile int shared_note_index = 0;
static volatile TickType_t current_note_start_tick = 0;

// --- Spectrum Visualization ---
#define NUM_BANDS 16
volatile float spectrum[NUM_BANDS];

// --- Song Data ---
const Song songs[] PROGMEM= { /* ... 歌曲数据 ... */ };
const int numSongs = sizeof(songs) / sizeof(songs[0]);
```
- `buzzerTaskHandle`, `ledTaskHandle`: FreeRTOS任务句柄，用于管理音乐播放和LED任务。
- `currentPlayMode`: 当前的播放模式（单曲循环、列表循环、随机播放）。
- `stopBuzzerTask`, `isPaused`, `stopLedTask`: 控制音乐和LED任务的停止和暂停状态的标志。
- `shared_song_index`, `shared_note_index`, `current_note_start_tick`: 任务和UI之间共享的变量，用于跟踪当前播放的歌曲、音符和音符开始时间，以便计算播放进度。
- `spectrum`: 存储伪频谱数据的数组，用于UI显示。
- `songs[] PROGMEM`: 存储所有歌曲数据的数组，`PROGMEM` 关键字指示数据存储在Flash中。
- `numSongs`: 歌曲总数。

### 4.2 伪频谱生成 `generate_fake_spectrum()`

```c++
void generate_fake_spectrum(int frequency) {
    for (int i = 0; i < NUM_BANDS; i++) { spectrum[i] = 0.0f; }
    if (frequency <= 0) return;
    const float min_log_freq = log(200);
    const float max_log_freq = log(2000);
    float log_freq = log(frequency);
    int band = NUM_BANDS * (log_freq - min_log_freq) / (max_log_freq - min_log_freq);
    if (band < 0) band = 0;
    if (band >= NUM_BANDS) band = NUM_BANDS - 1;
    spectrum[band] = 70.0f;
    if (band > 0) spectrum[band - 1] = 35.0f;
    if (band < NUM_BANDS - 1) spectrum[band + 1] = 35.0f;
}
```
- 此函数根据当前播放音符的频率生成一个简单的伪频谱数据。
- 它将频率映射到 `NUM_BANDS` 个频段中的一个，并为该频段及其相邻频段设置不同的强度值。
- 这种方法提供了一种轻量级的视觉效果，而无需复杂的FFT计算。

### 4.3 音乐播放任务 `Buzzer_Task()`

```c++
void Buzzer_Task(void *pvParameters) {
  int songIdx = *(int*)pvParameters;
  for(;;) {
    shared_song_index = songIdx;
    Song song;
    memcpy_P(&song, &songs[songIdx], sizeof(Song));
    for (int i = 0; i < song.length; i++) {
      shared_note_index = i;
      current_note_start_tick = xTaskGetTickCount();
      if (stopBuzzerTask) { noTone(BUZZER_PIN); vTaskDelete(NULL); }
      while (isPaused) {
        generate_fake_spectrum(0);
        noTone(BUZZER_PIN);
        current_note_start_tick = xTaskGetTickCount();
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      int note = pgm_read_word(song.melody+i);
      int duration = pgm_read_word(song.durations+i);
      generate_fake_spectrum(note);
      if (note > 0) { tone(BUZZER_PIN, note, duration*0.9); }
      vTaskDelay(pdMS_TO_TICKS(duration));
    }
    if (currentPlayMode == SINGLE_LOOP) {} // Stay on current song
    else if (currentPlayMode == LIST_LOOP) { songIdx = (songIdx + 1) % numSongs; }
    else if (currentPlayMode == RANDOM_PLAY) {
      if (numSongs > 1) { int currentSong = songIdx; do { songIdx = random(numSongs); } while (songIdx == currentSong); }
    }
  }
}
```
- 这是一个FreeRTOS任务，负责实际的音乐播放。
- 它会根据 `songIdx` 播放当前歌曲的每个音符，并更新 `shared_song_index` 和 `shared_note_index`。
- 在播放每个音符前，会检查 `stopBuzzerTask` 和 `isPaused` 标志，以响应停止或暂停请求。
- `generate_fake_spectrum(note)`: 在播放音符时更新伪频谱数据。
- 歌曲播放结束后，根据 `currentPlayMode` 决定下一首播放的歌曲。

### 4.4 LED彩虹任务 `Led_Rainbow_Task()`

```c++
void Led_Rainbow_Task(void *pvParameters) {
    for (uint16_t j = 0; ; j++) {
        if (stopLedTask) {
            strip.clear();
            strip.show();
            vTaskDelete(NULL);
        }
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```
- 这是一个FreeRTOS任务，负责控制LED灯带显示彩虹效果。
- `Wheel()` 函数用于根据位置生成彩虹颜色。
- 任务会循环更新每个LED的颜色，并调用 `strip.show()` 来刷新LED灯带。
- `stopLedTask` 标志用于控制任务的停止。

### 4.5 UI 绘制 `displaySongList()` 与 `displayPlayingSong()`

```c++
void displaySongList(int selectedIndex) { /* ... */ }
void displayPlayingSong() { /* ... */ }
```
- `displaySongList()`: 绘制歌曲选择列表界面，高亮显示当前选中歌曲。
- `displayPlayingSong()`: 绘制音乐播放界面，显示歌曲名称、实时时间、播放模式、伪频谱、播放进度条和已播放/总时长。
- 两个函数都使用 `menuSprite` 进行双缓冲绘制。

### 4.6 主音乐菜单函数 `BuzzerMenu()`

```c++
void BuzzerMenu() {
  selectedSongIndex = 0;
  displayOffset = 0;
  isPaused = false;
  displaySongList(selectedSongIndex);
  while (1) { /* ... 歌曲选择循环 ... */ }

  stopBuzzerTask = false;
  stopLedTask = false;
  isPaused = false;
  currentPlayMode = LIST_LOOP;
  xTaskCreatePinnedToCore(Buzzer_Task, "Buzzer_Task", 4096, &selectedSongIndex, 2, &buzzerTaskHandle, 0);
  xTaskCreatePinnedToCore(Led_Rainbow_Task, "Led_Rainbow_Task", 2048, NULL, 1, &ledTaskHandle, 0);

  unsigned long lastScreenUpdateTime = 0;
  while (1) { /* ... 播放控制循环 ... */ }
}
```
- `BuzzerMenu()` 是音乐模块的主入口函数，它包含两个主要阶段：
    1.  **歌曲选择阶段**: 用户通过旋转编码器选择歌曲，单击按键确认选择。在此阶段，`displaySongList()` 负责UI绘制。
    2.  **音乐播放阶段**: 选定歌曲后，创建 `Buzzer_Task` 和 `Led_Rainbow_Task` 任务开始播放音乐和LED效果。用户可以通过按键暂停/恢复，旋转编码器切换播放模式。在此阶段，`displayPlayingSong()` 负责UI绘制。
- **退出逻辑**: 
    - 长按按键或闹钟响铃 (`g_alarm_is_ringing`) 会停止所有音乐和LED任务，并退出 `BuzzerMenu`。
    - `stop_buzzer_playback()` 函数负责清理所有任务和资源。

## 五、总结与展望

音乐模块通过FreeRTOS多任务、PROGMEM存储和直观的UI设计，为多功能时钟提供了丰富的音乐播放体验。伪频谱和LED视觉化效果进一步提升了交互的趣味性。

未来的改进方向：
1.  **更复杂的频谱分析**: 引入FFT（快速傅里叶变换）算法，实现更真实的音频频谱视觉化。
2.  **自定义歌曲上传**: 允许用户通过SD卡或网络上传自定义的MIDI或音符序列。
3.  **音量控制**: 增加音量调节功能，通过PWM或数字电位器实现。
4.  **更多LED效果**: 除了彩虹效果，可以增加更多与音乐节奏同步的LED灯效。
5.  **蓝牙音频输入**: 允许设备通过蓝牙接收音频流并播放。

谢谢大家
