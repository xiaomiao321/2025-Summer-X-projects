#include "buzzer.h"
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include <time.h>
#include "LED.h"


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
  int colorSchemeIndex; // New: Index to colorSchemes array
} Song;



// 歌曲列表
Song songs[] = {
  {"Da Hai", melody_da_hai, durations_da_hai, sizeof(melody_da_hai) / sizeof(melody_da_hai[0]), 0}, // Scheme 0
  {"Happy Birthday", melody_happy_birthday, durations_happy_birthday, sizeof(melody_happy_birthday) / sizeof(melody_happy_birthday[0]), 1}, // Scheme 1
  {"Yu Jian", melody_yu_jian, durations_yu_jian, sizeof(melody_yu_jian) / sizeof(melody_yu_jian[0]), 2}, // Scheme 2
  {"Two Tigers", melody_two_tigers, durations_two_tigers, sizeof(melody_two_tigers) / sizeof(melody_two_tigers[0]), 3}, // Scheme 3
  {"Mary Had a Lamb", melody_mary_lamb, durations_mary_lamb, sizeof(melody_mary_lamb) / sizeof(melody_mary_lamb[0]), 4}, // Scheme 4
  {"Jingle Bells", melody_jingle_bells, durations_jingle_bells, sizeof(melody_jingle_bells) / sizeof(melody_jingle_bells[0]), 0} // Scheme 0
};
const int numSongs = sizeof(songs) / sizeof(songs[0]);

// Color scheme structure
typedef struct {
  uint16_t bgColorStart; // Background gradient start color
  uint16_t bgColorEnd;   // Background gradient end color
  uint16_t frameColor1;  // Frame color 1
  uint16_t frameColor2;  // Frame color 2
  uint16_t frameColor3;  // Frame color 3
  uint16_t textColor;    // Normal text color
  uint16_t highlightColor; // Highlighted text color
} ColorScheme;

// Define some color schemes
const ColorScheme colorSchemes[] = {
  // Scheme 0: Default Blue
  {TFT_BLUE, TFT_CYAN, TFT_CYAN, TFT_WHITE, TFT_BLUE, TFT_WHITE, TFT_YELLOW},
  // Scheme 1: Greenish
  {TFT_DARKGREEN, TFT_GREEN, TFT_GREEN, TFT_WHITE, TFT_GREENYELLOW, TFT_WHITE, TFT_ORANGE}, // TFT_LIME replaced with TFT_GREENYELLOW
  // Scheme 2: Purplish
  {TFT_PURPLE, TFT_MAGENTA, TFT_MAGENTA, TFT_WHITE, TFT_PINK, TFT_WHITE, TFT_CYAN},
  // Scheme 3: Reddish
  {TFT_MAROON, TFT_RED, TFT_RED, TFT_WHITE, TFT_ORANGE, TFT_WHITE, TFT_YELLOW}, // TFT_DARKRED replaced with TFT_MAROON
  // Scheme 4: Greyish
  {TFT_DARKGREY, TFT_LIGHTGREY, TFT_LIGHTGREY, TFT_WHITE, TFT_SILVER, TFT_WHITE, TFT_YELLOW},
};
const int numColorSchemes = sizeof(colorSchemes) / sizeof(colorSchemes[0]);

// Map frequency to a more distinct color for NeoPixel
uint32_t mapFrequencyToColor(int frequency) {
  if (frequency == 0) return strip.Color(0, 0, 0); // Rest note is off

  // Define frequency bands and corresponding colors
  // Frequencies are roughly 100-2000 Hz
  if (frequency < 300) { // Low notes
    return strip.Color(255, 0, 0); // Red
  } else if (frequency < 600) { // Mid-low notes
    return strip.Color(255, 128, 0); // Orange
  } else if (frequency < 1000) { // Mid notes
    return strip.Color(0, 255, 0); // Green
  } else if (frequency < 1500) { // Mid-high notes
    return strip.Color(0, 0, 255); // Blue
  } else { // High notes
    return strip.Color(128, 0, 255); // Purple
  }
}

// Helper function to interpolate 565 colors
uint16_t interpolateColor(uint16_t color1, uint16_t color2, float ratio) {
  uint8_t r1 = (color1 >> 11) & 0x1F;
  uint8_t g1 = (color1 >> 5) & 0x3F;
  uint8_t b1 = (color1) & 0x1F;

  uint8_t r2 = (color2 >> 11) & 0x1F;
  uint8_t g2 = (color2 >> 5) & 0x3F;
  uint8_t b2 = (color2) & 0x1F;

  uint8_t r = r1 + (r2 - r1) * ratio;
  uint8_t g = g1 + (g2 - g1) * ratio;
  uint8_t b = b1 + (b2 - b1) * ratio;

  return tft.color565(r, g, b);
}

// LED effects (blocking versions, to be run in separate task)
void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
}

void rainbow(int wait) {
  for (long firstPixelHue = 0; firstPixelHue < 3 * 65536; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
}

bool stopBuzzerTask = false; // Moved declaration to here

// LED effects from LED.cpp, adapted for Buzzer.cpp


int selectedSongIndex = 0;
int displayOffset = 0; // 滚动偏移
const int visibleSongs = 3; // 屏幕可见歌曲数
bool firstDraw = true; // 全局绘制标志
bool isPaused = false; // Flag to control pause/resume

// Communication struct for LED Task
typedef struct {
  int effectType; // 0: None, 1: ColorWipe, 2: Rainbow, 3: FillColor
  uint32_t color;
  int frequency; // For frequency-based effects
} LedEffectCommand;

LedEffectCommand currentLedCommand = {0, 0, 0}; // Initialize with no effect
bool stopLedTask = false; // Flag to stop Led_Task
TaskHandle_t ledTaskHandle = NULL; // Handle for Led_Task

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
  // Get current color scheme
  const ColorScheme& currentScheme = colorSchemes[songs[selectedIndex].colorSchemeIndex];

  // Background gradient
  for (int y = 0; y < 240; y++) {
    uint16_t interpolatedColor = interpolateColor(currentScheme.bgColorStart, currentScheme.bgColorEnd, (float)y / 239.0);
    tft.drawFastHLine(0, y, 240, interpolatedColor);
  }

  // Dynamic border
  static int frameColorIndex = 0;
  uint16_t frameColors[] = {currentScheme.frameColor1, currentScheme.frameColor2, currentScheme.frameColor3};
  tft.drawRect(5, 5, 230, 230, frameColors[frameColorIndex % 3]);
  frameColorIndex++;

  tft.setTextColor(currentScheme.textColor, TFT_TRANSPARENT); // Use scheme text color
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

  tft.setTextWrap(true);
  for (int i = 0; i < visibleSongs; i++) {
    int songIdx = displayOffset + i;
    if (songIdx >= numSongs) break;
    tft.setCursor(10, 60 + i * 40);
    if (songIdx == selectedIndex) {
      tft.setTextSize(2);
      tft.setTextColor(currentScheme.highlightColor, TFT_TRANSPARENT); // Use scheme highlight color
      tft.print("> ");
      tft.println(songs[songIdx].name);
      tft.setTextSize(1);
      tft.setTextColor(currentScheme.textColor, TFT_TRANSPARENT); // Use scheme text color for non-selected
    } else {
      tft.setTextSize(1);
      tft.println(songs[songIdx].name);
    }
  }
}

// 显示播放界面
void displayPlayingSong(int songIndex, int noteIndex, int totalNotes, int currentNote, const ColorScheme& scheme) {
  static int lastNoteIndex = -1;
  static int wavePoints[48][2]; // 存储上一帧波形点（240/5=48 个点）

  if (firstDraw) {
    // 首次绘制背景和静态元素
    // 渐变背景
    for (int y = 0; y < 240; y++) {
      uint16_t interpolatedColor = interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)y / 239.0);
      tft.drawFastHLine(0, y, 240, interpolatedColor);
    }
    tft.setTextColor(scheme.textColor, TFT_TRANSPARENT); // Use scheme text color
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.print("Playing: ");
    tft.println(songs[songIndex].name);

    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.printf("Duration: %.1f s", calculateSongDuration(&songs[songIndex]));

    // 进度条背景
    tft.fillRect(20, 130, 200, 10, TFT_DARKGREY); // Keep darkgrey for now, can be scheme color later

    // 初始化波形区域
    tft.fillRect(0, 160, 240, 60, interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)160 / 239.0)); // Use scheme background color
    firstDraw = false;
  } else if (noteIndex != lastNoteIndex) {
    // 更新动态部分
    // 动态边框
    static int frameColorIndex = 0;
    uint16_t frameColors[] = {scheme.frameColor1, scheme.frameColor2, scheme.frameColor3}; // Use scheme frame colors
    tft.drawRect(5, 5, 230, 230, frameColors[frameColorIndex % 3]);
    frameColorIndex++;

    // 更新音符进度
    tft.fillRect(10, 80, 200, 20, interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)80 / 239.0)); // Clear with scheme background
    tft.setTextSize(1);
    tft.setTextColor(scheme.textColor, TFT_TRANSPARENT); // Use scheme text color
    tft.setCursor(10, 80);
    tft.printf("Note: %d/%d", noteIndex + 1, totalNotes);

    // 更新进度条
    int progressWidth = (noteIndex + 1) * 200 / totalNotes;
    tft.fillRect(20, 130, progressWidth, 10, scheme.highlightColor); // Use scheme highlight color for progress
    // Clear remaining part of progress bar
    tft.fillRect(20 + progressWidth, 130, 200 - progressWidth, 10, TFT_DARKGREY); // Keep darkgrey for now

    // 清除旧波形
    for (int i = 0; i < 48; i++) {
      if (wavePoints[i][0] >= 0) { // 有效点
        tft.fillCircle(wavePoints[i][0], wavePoints[i][1], 3, interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)wavePoints[i][1] / 239.0)); // Clear with scheme background
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
    tft.fillCircle(x, y, 3, scheme.highlightColor); // Use scheme highlight color for waveform
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

// LED Animation Task
void Led_Task(void *pvParameters) {
  static int colorWipePixel = 0; // State for colorWipe
  static long rainbowHue = 0;    // State for rainbow
  static int currentEffect = 0;  // 0: None, 1: ColorWipe, 2: Rainbow, 3: FillColor

  for (;;) {
    if (stopLedTask) { // Check stop flag
      strip.clear();
      strip.show();
      vTaskDelete(NULL); // Delete task
    }

    // Process new command (only if no effect is currently running)
    if (currentLedCommand.effectType != 0 && currentEffect == 0) {
      currentEffect = currentLedCommand.effectType; // Set current effect
      // Reset state for new effect
      colorWipePixel = 0;
      rainbowHue = 0;

      if (currentEffect == 3) { // FillColor is instant
        strip.fill(currentLedCommand.color);
        strip.show();
        currentEffect = 0; // Mark as finished
      }
      currentLedCommand.effectType = 0; // Reset command after processing
    }

    // Continue current effect (if any)
    if (currentEffect == 1) { // ColorWipe (step by step)
      if (colorWipePixel < strip.numPixels()) {
        strip.setPixelColor(colorWipePixel, currentLedCommand.color);
        strip.show();
        colorWipePixel++;
      } else {
        // Effect finished
        vTaskDelay(pdMS_TO_TICKS(50)); // Hold for a moment
        strip.clear();
        strip.show();
        currentEffect = 0; // Mark as finished
      }
    } else if (currentEffect == 2) { // Rainbow (step by step)
      if (rainbowHue < 3 * 65536) { // Continue rainbow
        for (int i = 0; i < strip.numPixels(); i++) {
          int pixelHue = rainbowHue + (i * 65536L / strip.numPixels());
          strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        }
        strip.show();
        rainbowHue += 256; // Advance hue
      } else {
        // Effect finished
        vTaskDelay(pdMS_TO_TICKS(50)); // Hold for a moment
        strip.clear();
        strip.show();
        currentEffect = 0; // Mark as finished
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
  }
}

// 蜂鸣器播放任务
void Buzzer_Task(void *pvParameters) {
  int songIndex = *(int*)pvParameters;
  Song* song = &songs[songIndex];
  for (;;) {
    if (stopBuzzerTask) {
      noTone(BUZZER_PIN);
      strip.clear(); // Clear LEDs on exit
      strip.show();  // Turn off LEDs
      stopBuzzerTask = false;
      Serial.println("Buzzer_Task 被外部中断，已停止");
      firstDraw = true; // 重置绘制标志
      break;
    }

    // Pause/Resume logic
    if (isPaused) {
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks while paused
      continue; // Skip playing notes
    }

    for (int i = 0; i < song->length; i++) {
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        strip.clear(); // Clear LEDs on exit
        strip.show();  // Turn off LEDs
        stopBuzzerTask = false;
        firstDraw = true;
        goto exit_loop;
      }
      tone(BUZZER_PIN, song->melody[i], song->durations[i]);
      // Send command to Led_Task
      currentLedCommand.effectType = (i % 2 == 0) ? 1 : 2; // Alternate colorWipe and rainbow
      currentLedCommand.color = mapFrequencyToColor(song->melody[i]);
      currentLedCommand.frequency = song->melody[i];
      
      displayPlayingSong(songIndex, i, song->length, song->melody[i], colorSchemes[song->colorSchemeIndex]);
      vTaskDelay(pdMS_TO_TICKS(song->durations[i] * 1.3));
      noTone(BUZZER_PIN);
      // Led_Task will handle clearing the strip after the effect
    }
    Serial.printf("%s 一轮播放完成\n", song->name);
    vTaskDelay(pdMS_TO_TICKS(2000));
    firstDraw = true; // 重置绘制标志
  }
exit_loop:
  strip.clear(); // Ensure LEDs are off when task deletes itself
  strip.show();
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
  strip.show();  // Initialize all pixels to 'off'
  stopBuzzerTask = false;
  stopLedTask = false; // Reset LED task stop flag
  selectedSongIndex = 0;
  displayOffset = 0;
  firstDraw = true; // 初始化绘制标志

  // 歌曲选择循环
select_song:
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
        stopLedTask = true; // Signal Led_Task to stop
        TaskHandle_t taskHandle = xTaskGetHandle("Led_Task");
        if (taskHandle != NULL) {
          vTaskDelete(taskHandle); // Delete Led_Task
        }
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
  xTaskCreatePinnedToCore(Led_Task, "Led_Task", 4096, NULL, 1, NULL, 0); // Start LED task

  // 播放控制循环
  while (1) {
    if (readButton()) {
      if (detectDoubleClick()) { // 双击返回主菜单
        Serial.println("双击检测到，返回主菜单");
        stopBuzzerTask = true;
        stopLedTask = true; // Signal Led_Task to stop
        TaskHandle_t buzzerTaskHandle = xTaskGetHandle("Buzzer_Task");
        if (buzzerTaskHandle != NULL) {
          const TickType_t xTimeout = pdMS_TO_TICKS(100);
          while (eTaskGetState(buzzerTaskHandle) != eDeleted && xTimeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        TaskHandle_t ledTaskHandle = xTaskGetHandle("Led_Task");
        if (ledTaskHandle != NULL) {
          vTaskDelete(ledTaskHandle); // Delete Led_Task
        }
        TaskHandle_t initHandle = xTaskGetHandle("Buzzer_Init");
        if (initHandle != NULL) {
          vTaskDelete(initHandle);
        }
        display = 48;
        picture_flag = 0;
        showMenuConfig();
        return;
      } else { // 单击：暂停/继续
        isPaused = !isPaused; // Toggle pause state
        if (isPaused) {
          Serial.println("音乐暂停");
          noTone(BUZZER_PIN); // Stop current note
          currentLedCommand.effectType = 3; // Fill with black (off)
          currentLedCommand.color = strip.Color(0,0,0);
        } else {
          Serial.println("音乐继续");
          // Optionally, resume LED effect here if it was paused
        }
        // Add a visual indicator for pause/play if desired
        // For now, just print to serial
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}