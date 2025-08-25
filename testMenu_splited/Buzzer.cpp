#include "buzzer.h"
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include <time.h>


// 音阶频率（Hz）
#define NOTE_REST 0
#define NOTE_G3 196
#define NOTE_A3 220
#define NOTE_B3 247
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988
// 音阶频率（Hz）
#define P0 	0	// 休止符频率

#define L1 262  // 低音频率
#define L2 294
#define L3 330
#define L4 349
#define L5 392
#define L6 440
#define L7 494

#define M1 523  // 中音频率
#define M2 587
#define M3 659
#define M4 698
#define M5 784
#define M6 880
#define M7 988

#define H1 1047 // 高音频率
#define H2 1175
#define H3 1319
#define H4 1397
#define H5 1568
#define H6 1760
#define H7 1976


#define DAHAI_TIME_OF_BEAT 714 // 大海节拍时间（ms）

#define YUJIAN_TIME_OF_BEAT 652 // 大海节拍时间（ms）


#define DOUBLE_CLICK_TIME 500 // 双击时间（ms）

// 大海
int melody_da_hai[] = {
  L5,L6,M1,M1,M1,M1,L6,L5,M1,M1,M2,M1,L6,M1,M2,M2,M2,M2,M1,L6,M2,M2,M3,M2,M3,M5,M6,M6,M5,M6,M5,M3,M2,M2,M3,M2,M1,L6,L5,L6,M1,M1,M1,M1,L6,M1,L5,L6,M1,M1,M1,M1,L6,L5,M1,M1,M2,M1,L6,M1,M2,M2,M2,M2,M1,L6,M2,M2,M3,M2,M3,M5,M6,M6,M5,M6,H1,M6,M5,M3,M2,M1,L6,L5,L6,M1,M1,M1,M1,M2,M1,M1,M3,M5
};
int durations_da_hai[] = {
  DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,
  DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/4,DAHAI_TIME_OF_BEAT/4,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT*3,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,
  
  DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT/2,DAHAI_TIME_OF_BEAT*3,DAHAI_TIME_OF_BEAT/2,
};

// 生日快乐
int melody_happy_birthday[] = {
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_F4, NOTE_E4,
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_G4, NOTE_F4,
  NOTE_C4, NOTE_C4, NOTE_C5, NOTE_A4, NOTE_F4, NOTE_E4, NOTE_D4,
  NOTE_A4, NOTE_A4, NOTE_A4, NOTE_F4, NOTE_G4, NOTE_F4
};
int durations_happy_birthday[] = {
  250, 250, 500, 500, 500, 1000,
  250, 250, 500, 500, 500, 1000,
  250, 250, 500, 500, 500, 500, 1000,
  250, 250, 500, 500, 500, 1000
};

// 遇见
int melody_yu_jian[] = {
    M5,M3,M3,M5,M2,M2,M3,M2,M2,M1,M1,M1,L7,L6,L7,M1,L7,M1,M1,M2,M3,M3,M5,M3,M3,M5,M2,M2,M3,M2,M2,M1,M1,M1,L7,L6,L7,M1,L7,L7,M1,M1,M2,M1
};
int durations_yu_jian[] = {
YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT*2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT*3,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT*2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT/2,YUJIAN_TIME_OF_BEAT*2
};

// 两只老虎
int melody_two_tigers[] = {
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_C4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_C4,
  NOTE_E4, NOTE_F4, NOTE_G4, NOTE_E4, NOTE_F4, NOTE_G4,
  NOTE_G4, NOTE_A4, NOTE_G4, NOTE_F4, NOTE_E4, NOTE_C4, NOTE_D4, NOTE_C4
};
int durations_two_tigers[] = {
  500, 500, 500, 500, 500, 500, 500, 500,
  500, 500, 1000, 500, 500, 1000,
  250, 250, 250, 250, 500, 500, 500, 1000
};

// Mary Had a Little Lamb
int melody_mary_lamb[] = {
  NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_D4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_G4, NOTE_G4,
  NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_D4, NOTE_D4, NOTE_E4, NOTE_D4, NOTE_C4
};
int durations_mary_lamb[] = {
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 1000, 500, 500, 1000,
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 500, 500, 1000
};

// Jingle Bells
int melody_jingle_bells[] = {
  NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_G4,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4,
  NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_E4,
  NOTE_D4, NOTE_G4
};
int durations_jingle_bells[] = {
  500, 500, 1000, 500, 500, 1000, 500, 500,
  500, 500, 1000, 500, 500, 500, 500, 500,
  500, 500, 500, 500, 500, 500, 500, 500,
  500, 1000
};

// 歌曲数据结构
typedef struct {
  const char* name; // 歌曲名称
  int* melody;      // 音符序列
  int* durations;   // 节拍时长
  int length;       // 音符数量
} Song;



// 歌曲列表
Song songs[] = {
  {"Da Hai", melody_da_hai, durations_da_hai, sizeof(melody_da_hai) / sizeof(melody_da_hai[0])},
  {"Happy Birthday", melody_happy_birthday, durations_happy_birthday, sizeof(melody_happy_birthday) / sizeof(melody_happy_birthday[0])},
  {"Yu Jian", melody_yu_jian, durations_yu_jian, sizeof(melody_yu_jian) / sizeof(melody_yu_jian[0])},
  {"Two Tigers", melody_two_tigers, durations_two_tigers, sizeof(melody_two_tigers) / sizeof(melody_two_tigers[0])},
  {"Mary Had a Lamb", melody_mary_lamb, durations_mary_lamb, sizeof(melody_mary_lamb) / sizeof(melody_mary_lamb[0])},
  {"Jingle Bells", melody_jingle_bells, durations_jingle_bells, sizeof(melody_jingle_bells) / sizeof(melody_jingle_bells[0])}
};
const int numSongs = sizeof(songs) / sizeof(songs[0]);

bool stopBuzzerTask = false;
int selectedSongIndex = 0;
int displayOffset = 0; // 滚动偏移
const int visibleSongs = 3; // 屏幕可见歌曲数
bool firstDraw = true; // 全局绘制标志

// 计算歌曲总时长（秒）
float calculateSongDuration(Song* song) {
  int totalMs = 0;
  for (int i = 0; i < song->length; i++) {
    totalMs += song->durations[i] * 1.3; // 包含 30% 停顿
  }
  return totalMs / 1000.0; // 转换为秒
}

// 初始化 ESP32 内部 RTC
void initInternalRTC() {
  struct tm timeinfo = {0};
  timeinfo.tm_year = 2025 - 1900; // 年份从 1900 开始
  timeinfo.tm_mon = 7; // 8月 (0-11)
  timeinfo.tm_mday = 20;
  timeinfo.tm_hour = 18;
  timeinfo.tm_min = 36;
  timeinfo.tm_sec = 0;

  time_t t = mktime(&timeinfo);
  struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  Serial.println("内部 RTC 初始化为 2025-08-20 18:36:00");
}

// 显示歌曲选择列表
void displaySongList(int selectedIndex) {
  // 渐变背景（深蓝到浅蓝）
  for (int y = 0; y < 240; y++) {
    uint16_t color = tft.color565(0, 0, 50 + y / 2); // 深蓝到浅蓝
    tft.drawFastHLine(0, y, 240, color);
  }
  // 动态边框（颜色循环）
  static int frameColorIndex = 0;
  uint16_t frameColors[] = {TFT_CYAN, TFT_WHITE, TFT_BLUE};
  tft.drawRect(5, 5, 230, 230, frameColors[frameColorIndex % 3]);
  frameColorIndex++;

  tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Song Menu");
  tft.setTextSize(1);
  tft.setCursor(10, 30);
  time_t now;
  time(&now);
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&now));
  tft.print("Time: ");
  tft.print(timeStr);

  tft.setTextWrap(true); // 启用换行
  for (int i = 0; i < visibleSongs; i++) {
    int songIdx = displayOffset + i;
    if (songIdx >= numSongs) break;
    tft.setCursor(10, 60 + i * 40); // 间隔 40
    if (songIdx == selectedIndex) {
      tft.setTextSize(2); // 选中歌曲稍大
      tft.setTextColor(TFT_YELLOW, TFT_TRANSPARENT);
      tft.print("> ");
      tft.println(songs[songIdx].name);
      tft.setTextSize(1); // 非选中歌曲
      tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
    } else {
      tft.setTextSize(1);
      tft.println(songs[songIdx].name);
    }
  }
}

// 显示播放界面
void displayPlayingSong(int songIndex, int noteIndex, int totalNotes, int currentNote) {
  static int lastNoteIndex = -1;
  static int wavePoints[48][2]; // 存储上一帧波形点（240/5=48 个点）

  if (firstDraw) {
    // 首次绘制背景和静态元素
    // 渐变背景
    for (int y = 0; y < 240; y++) {
      uint16_t color = tft.color565(0, 50 + y / 2, 0); // 深绿到浅绿
      tft.drawFastHLine(0, y, 240, color);
    }
    tft.setTextColor(TFT_GREEN, TFT_TRANSPARENT);
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.print("Playing: ");
    tft.println(songs[songIndex].name);

    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.printf("Duration: %.1f s", calculateSongDuration(&songs[songIndex]));

    // 进度条背景
    tft.fillRect(20, 130, 200, 10, TFT_DARKGREY);

    // 初始化波形区域
    tft.fillRect(0, 160, 240, 60, tft.color565(0, 50 + 160 / 2, 0)); // 使用背景色
    firstDraw = false;
  } else if (noteIndex != lastNoteIndex) {
    // 更新动态部分
    // 动态边框
    static int frameColorIndex = 0;
    uint16_t frameColors[] = {TFT_CYAN, TFT_GREEN, TFT_BLUE};
    tft.drawRect(5, 5, 230, 230, frameColors[frameColorIndex % 3]);
    frameColorIndex++;

    // 更新音符进度
    tft.fillRect(10, 80, 200, 20, tft.color565(0, 50 + 80 / 2, 0)); // 覆盖旧进度文本
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_TRANSPARENT);
    tft.setCursor(10, 80);
    tft.printf("Note: %d/%d", noteIndex + 1, totalNotes);

    // 更新进度条
    int progressWidth = (noteIndex + 1) * 200 / totalNotes;
    tft.fillRect(20, 130, progressWidth, 10, TFT_BLUE);

    // 清除旧波形
    for (int i = 0; i < 48; i++) {
      if (wavePoints[i][0] >= 0) { // 有效点
        tft.fillCircle(wavePoints[i][0], wavePoints[i][1], 3, tft.color565(0, 50 + 160 / 2, 0)); // 背景色
      }
    }

    lastNoteIndex = noteIndex;
  }

  // 动态波形动画，根据音符音调
  float freq = currentNote ? currentNote : 262; // 避免 0 频率
  float amplitude = 20.0 * (freq / 523.0); // 幅度随频率
  static int waveOffset = 0;
  waveOffset = (waveOffset + 1) % 240;
  for (int i = 0; i < 48; i++) {
    int x = i * 5;
    int y = 190 + sin((x + waveOffset) * 0.1 * (freq / 100.0)) * amplitude;
    tft.fillCircle(x, y, 3, TFT_YELLOW);
    wavePoints[i][0] = x;
    wavePoints[i][1] = y;
  }
}

// 蜂鸣器初始化任务
void Buzzer_Init_Task(void *pvParameters) {
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("无源蜂鸣器初始化完成");
  vTaskDelete(NULL);
}

// 蜂鸣器播放任务
void Buzzer_Task(void *pvParameters) {
  int songIndex = *(int*)pvParameters;
  Song* song = &songs[songIndex];
  for (;;) {
    if (stopBuzzerTask) {
      noTone(BUZZER_PIN);
      stopBuzzerTask = false;
      Serial.println("Buzzer_Task 被外部中断，已停止");
      firstDraw = true; // 重置绘制标志
      break;
    }
    for (int i = 0; i < song->length; i++) {
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        stopBuzzerTask = false;
        firstDraw = true;
        goto exit_loop;
      }
      tone(BUZZER_PIN, song->melody[i], song->durations[i]);
      displayPlayingSong(songIndex, i, song->length, song->melody[i]);
      vTaskDelay(pdMS_TO_TICKS(song->durations[i] * 1.3));
      noTone(BUZZER_PIN);
    }
    Serial.printf("%s 一轮播放完成\n", song->name);
    vTaskDelay(pdMS_TO_TICKS(2000));
    firstDraw = true; // 重置绘制标志
  }
exit_loop:
  vTaskDelete(NULL);
}

// 双击检测
bool detectDoubleClick() {
  static unsigned long lastClickTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastClickTime < 500) { // 500ms 内两次点击
    lastClickTime = 0;
    return true;
  }
  lastClickTime = currentTime;
  return false;
}

// 蜂鸣器菜单函数
void BuzzerMenu() {
  tft.init();
  tft.setRotation(1); // 调整屏幕方向
  tft.fillScreen(TFT_BLACK);
  initRotaryEncoder();
  initInternalRTC(); // 初始化内部 RTC
  stopBuzzerTask = false;
  selectedSongIndex = 0;
  displayOffset = 0;
  firstDraw = true; // 初始化绘制标志

  // 歌曲选择循环
select_song:
  animateMenuTransition("BUZZER", true);
  displaySongList(selectedSongIndex);

  while (1) {
    int encoderChange = readEncoder();
    if (encoderChange != 0) {
      selectedSongIndex = (selectedSongIndex + encoderChange + numSongs) % numSongs;
      // 更新滚动偏移
      if (selectedSongIndex < displayOffset) {
        displayOffset = selectedSongIndex;
      } else if (selectedSongIndex >= displayOffset + visibleSongs) {
        displayOffset = selectedSongIndex - visibleSongs + 1;
      }
      displaySongList(selectedSongIndex);
      tone(BUZZER_PIN, 1000, 50); // Add a short tone when switching songs
      Serial.printf("当前选择歌曲：%s\n", songs[selectedSongIndex].name);
    }
    if (readButton()) {
      if (detectDoubleClick()) { // 双击返回主菜单
        Serial.println("双击检测到，返回主菜单");
        animateMenuTransition("BUZZER", false);
        display = 48;
        picture_flag = 0;
        showMenuConfig();
        return;
      }
      Serial.printf("确认播放歌曲：%s\n", songs[selectedSongIndex].name);
      firstDraw = true; // 重置绘制标志
      break; // 单击进入播放
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // 启动播放任务
  xTaskCreatePinnedToCore(Buzzer_Init_Task, "Buzzer_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(Buzzer_Task, "Buzzer_Task", 8192, &selectedSongIndex, 1, NULL, 0);

  // 播放控制循环
  while (1) {
    if (readButton()) {
      if (detectDoubleClick()) { // 双击返回主菜单
        Serial.println("双击检测到，返回主菜单");
        stopBuzzerTask = true;
        TaskHandle_t taskHandle = xTaskGetHandle("Buzzer_Task");
        if (taskHandle != NULL) {
          const TickType_t xTimeout = pdMS_TO_TICKS(100);
          while (eTaskGetState(taskHandle) != eDeleted && xTimeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        TaskHandle_t initHandle = xTaskGetHandle("Buzzer_Init");
        if (initHandle != NULL) {
          vTaskDelete(initHandle);
        }
        animateMenuTransition(songs[selectedSongIndex].name, false);
        display = 48;
        picture_flag = 0;
        showMenuConfig();
        return;
      } else { // 单击返回歌曲选择
        Serial.println("单击检测到，返回歌曲选择");
        stopBuzzerTask = true;
        TaskHandle_t taskHandle = xTaskGetHandle("Buzzer_Task");
        if (taskHandle != NULL) {
          const TickType_t xTimeout = pdMS_TO_TICKS(100);
          while (eTaskGetState(taskHandle) != eDeleted && xTimeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        TaskHandle_t initHandle = xTaskGetHandle("Buzzer_Init");
        if (initHandle != NULL) {
          vTaskDelete(initHandle);
        }
        firstDraw = true; // 重置绘制标志
        goto select_song;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}