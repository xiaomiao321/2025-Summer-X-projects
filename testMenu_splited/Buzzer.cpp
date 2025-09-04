#include "Buzzer.h"
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include <time.h>
#include "LED.h"
#include "arduinoFFT.h"
#include <math.h>
#include "Menu.h"
#include "MQTT.h"
#include "weather.h"
// FFT Constants
#define SAMPLES 256
#define SAMPLING_FREQUENCY 4000
#define NUM_BANDS 16

// FFT Globals
ArduinoFFT<double> FFT = ArduinoFFT<double>();
double vReal[SAMPLES];
double vImag[SAMPLES];
double spectrum[NUM_BANDS];

// 播放模式枚举
enum PlayMode {
  SINGLE_LOOP,   // 单曲循环
  LIST_LOOP,     // 列表播放
  RANDOM_PLAY    // 随机播放
};

PlayMode currentPlayMode = LIST_LOOP; // 默认列表播放

// 0-4
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
  {TFT_DARKGREEN, TFT_GREEN, TFT_GREEN, TFT_WHITE, TFT_GREENYELLOW, TFT_WHITE, TFT_ORANGE},
  // Scheme 2: Purplish
  {TFT_PURPLE, TFT_MAGENTA, TFT_MAGENTA, TFT_WHITE, TFT_PINK, TFT_WHITE, TFT_CYAN},
  // Scheme 3: Reddish
  {TFT_MAROON, TFT_RED, TFT_RED, TFT_WHITE, TFT_ORANGE, TFT_WHITE, TFT_YELLOW},
  // Scheme 4: Greyish
  {TFT_DARKGREY, TFT_LIGHTGREY, TFT_LIGHTGREY, TFT_WHITE, TFT_SILVER, TFT_WHITE, TFT_YELLOW},
};
const int numColorSchemes = sizeof(colorSchemes) / sizeof(colorSchemes[0]);

// Map frequency to a more distinct color for NeoPixel
uint32_t mapFrequencyToColor(int frequency) {
  if (frequency == 0) return strip.Color(0, 0, 0); // Rest note is off

  // Define frequency bands and corresponding colors
  if (frequency < 300) {
    return strip.Color(255, 0, 0); // Red
  } else if (frequency < 600) {
    return strip.Color(255, 128, 0); // Orange
  } else if (frequency < 1000) {
    return strip.Color(0, 255, 0); // Green
  } else if (frequency < 1500) {
    return strip.Color(0, 0, 255); // Blue
  } else {
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

bool stopBuzzerTask = false;
bool isPaused = false;

int selectedSongIndex = 0;
int displayOffset = 0;
const int visibleSongs = 3;
bool firstDraw = true;

int songToPlayIndex = -1;

// Communication struct for LED Task
typedef struct {
  int effectType;
  uint32_t color;
  int frequency;
} LedEffectCommand;

LedEffectCommand currentLedCommand = {0, 0, 0};
bool stopLedTask = false;
TaskHandle_t ledTaskHandle = NULL;

// 计算歌曲总时长（秒）
float calculateSongDuration(const Song* song) {
  int totalMs = 0;
  for (int i = 0; i < song->length; i++) {
    int duration = pgm_read_word(song->durations+i);
    totalMs += duration * 1.3;
  }
  return totalMs / 1000.0;
}

// 颜色变暗函数
uint16_t darkenColor(uint16_t color, float factor) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    r = constrain(r * (1 - factor), 0, 31);
    g = constrain(g * (1 - factor), 0, 63);
    b = constrain(b * (1 - factor), 0, 31);
    
    return (r << 11) | (g << 5) | b;
}

// 显示歌曲选择列表
void displaySongList(int selectedIndex) {
  const ColorScheme& currentScheme = colorSchemes[songs[selectedIndex].colorSchemeIndex];
  
  // 绘制渐变背景
  for (int y = 0; y < 240; y++) {
    uint16_t interpolatedColor = interpolateColor(
      currentScheme.bgColorStart, 
      currentScheme.bgColorEnd, 
      (float)y / 239.0
    );
    tft.drawFastHLine(0, y, 240, interpolatedColor);
  }
  
  // 绘制动态边框
  static int frameColorIndex = 0;
  uint16_t frameColors[] = {
    currentScheme.frameColor1, 
    currentScheme.frameColor2, 
    currentScheme.frameColor3
  };
  tft.drawRoundRect(5, 5, 230, 230, 8, frameColors[frameColorIndex % 3]);
  frameColorIndex++;
  
  // 绘制标题区域
  tft.fillRoundRect(10, 10, 220, 35, 5, interpolateColor(
    currentScheme.bgColorStart, 
    currentScheme.bgColorEnd, 
    0.1f
  ));
  
  // 设置标题"歌曲菜单"居中显示
  tft.setTextColor(currentScheme.highlightColor, TFT_TRANSPARENT);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("♪ 歌曲菜单 ♪", 120, 28);
  
  // 绘制歌曲列表
  tft.setTextWrap(true);
  for (int i = 0; i < visibleSongs; i++) {
    int songIdx = displayOffset + i;
    if (songIdx >= numSongs) break;
   
    int yPos = 60 + i * 50;
    
    // 清除行背景，使用渐变效果
    uint16_t rowBgColor = interpolateColor(
      currentScheme.bgColorStart, 
      currentScheme.bgColorEnd, 
      (float)yPos/239.0
    );
    
    // 为每一行添加圆角矩形背景
    tft.fillRoundRect(10, yPos - 18, 220, 36, 5, rowBgColor);
    
    // 选中的歌曲高亮显示
    if (songIdx == selectedIndex) {
      // 使用更深的颜色作为高亮背景，确保文字清晰
      uint16_t darkHighlight = darkenColor(currentScheme.highlightColor, 0.3);
      tft.fillRoundRect(10, yPos - 18, 220, 36, 5, darkHighlight);
      
      // 歌曲名称 - 白色文字，透明背景，居中显示
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(songs[songIdx].name, 120, yPos);
      
      // 添加播放指示器
      tft.fillCircle(210, yPos, 5, TFT_WHITE);
      tft.fillCircle(210, yPos, 3, darkHighlight);
    } else {
      // 未选中的歌曲 - 使用主题文本色，透明背景，居中显示
      tft.setTextSize(1);
      tft.setTextColor(currentScheme.textColor, TFT_TRANSPARENT);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(songs[songIdx].name, 120, yPos);
      
      // 添加列表标记
      tft.fillCircle(15, yPos, 2, currentScheme.textColor);
    }
  }
  
  // // 添加滚动指示器（如果有更多歌曲）
  // if (numSongs > visibleSongs) {
  //   int scrollPos = map(displayOffset, 0, numSongs - visibleSongs, 200, 220);
  //   tft.fillRoundRect(225, scrollPos, 10, 20, 3, currentScheme.highlightColor);
  // }
  
  // 重置文本对齐方式
  tft.setTextDatum(TL_DATUM);
}


void drawSpectrum(const ColorScheme& scheme) {
  tft.fillRect(5, 165, 230, 70, interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)165 / 239.0));

  int barWidth = 230 / NUM_BANDS;
  for (int i = 0; i < NUM_BANDS; i++) {
    int barHeight = spectrum[i] * 100;
    if (barHeight > 70) {
      barHeight = 70;
    }
    uint16_t barColor = TFT_GREEN;
    tft.fillRect(5 + i * barWidth, 235 - barHeight, barWidth - 2, barHeight, barColor);
  }
}

// 显示播放界面
void displayPlayingSong(int songIndex, int noteIndex, int totalNotes, int currentNote, const ColorScheme& scheme) {
    if (!getLocalTime(&timeinfo)) {
      // Handle error if time is not available
      return;
    }
    static int lastNoteIndex = -1;
    static PlayMode lastPlayMode = currentPlayMode;
    static char lastTimeStr[20] = ""; // 保存上次显示的时间字符串

    // 格式化当前时间
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

    tft.fillRect(10, 50, 200, 40, interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)50/239.0));
    tft.setTextSize(1);
    tft.setTextColor(scheme.textColor, TFT_TRANSPARENT);

    // 显示时间
    tft.setCursor(10, 60);
    tft.println(timeStr);

    // 显示播放模式
    tft.setCursor(10, 70);
    switch (currentPlayMode) {
        case SINGLE_LOOP: tft.print("Single Loop"); break;
        case LIST_LOOP: tft.print("List Loop"); break;
        case RANDOM_PLAY: tft.print("Random"); break;
    }
    lastPlayMode = currentPlayMode;
    strcpy(lastTimeStr, timeStr); // 更新上次时间字符串

    if (firstDraw) {
        for (int y = 0; y < 240; y++) {
            uint16_t interpolatedColor = interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)y / 239.0);
            tft.drawFastHLine(0, y, 240, interpolatedColor);
        }
        tft.setTextColor(scheme.textColor, TFT_TRANSPARENT);
        tft.setTextSize(2);
        tft.setCursor(10, 30);
        tft.println(songs[songIndex].name);

        tft.setTextSize(1);
        tft.setCursor(10, 100);
        tft.printf("Duration: %.1f s", calculateSongDuration(&songs[songIndex]));

        tft.fillRect(20, 130, 200, 10, TFT_DARKGREY);

        firstDraw = false;
    } else if (noteIndex != lastNoteIndex) {
        static int frameColorIndex = 0;
        uint16_t frameColors[] = {scheme.frameColor1, scheme.frameColor2, scheme.frameColor3};
        tft.drawRect(5, 5, 230, 230, frameColors[frameColorIndex % 3]);
        frameColorIndex++;

        tft.fillRect(10, 80, 200, 20, interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)80 / 239.0));
        tft.setTextSize(1);
        tft.setTextColor(scheme.textColor, TFT_TRANSPARENT);
        tft.setCursor(10, 80);
        tft.printf("Note: %d/%d", noteIndex + 1, totalNotes);

        int progressWidth = (noteIndex + 1) * 200 / totalNotes;
        tft.fillRect(20, 130, progressWidth, 10, scheme.highlightColor);
        tft.fillRect(20 + progressWidth, 130, 200 - progressWidth, 10, TFT_DARKGREY);

        lastNoteIndex = noteIndex;
    }

    drawSpectrum(scheme);
}

// 蜂鸣器初始化
void Buzzer_Init() {
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("无源蜂鸣器初始化完成");
}

// LED Animation Task
void Led_Task(void *pvParameters) {
  static int colorWipePixel = 0;
  static long rainbowHue = 0;
  static int currentEffect = 0;

  for (;;) {
    if (stopLedTask) {
      strip.clear();
      strip.show();
      vTaskDelete(NULL);
      stopLedTask = false; // 重置标志
      vTaskDelete(NULL);
    }

    if (currentLedCommand.effectType != 0 && currentEffect == 0) {
      currentEffect = currentLedCommand.effectType;
      colorWipePixel = 0;
      rainbowHue = 0;

      if (currentEffect == 3) {
        strip.fill(currentLedCommand.color);
        strip.show();
        currentEffect = 0;
      }
      currentLedCommand.effectType = 0;
    }

    if (currentEffect == 1) {
      if (colorWipePixel < strip.numPixels()) {
        strip.setPixelColor(colorWipePixel, currentLedCommand.color);
        strip.show();
        colorWipePixel++;
      } else {
        vTaskDelay(pdMS_TO_TICKS(50));
        strip.clear();
        strip.show();
        currentEffect = 0;
      }
    } else if (currentEffect == 2) {
      if (rainbowHue < 3 * 65536) {
        for (int i = 0; i < strip.numPixels(); i++) {
          int pixelHue = rainbowHue + (i * 65536L / strip.numPixels());
          strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        }
        strip.show();
        rainbowHue += 256;
      } else {
        vTaskDelay(pdMS_TO_TICKS(50));
        strip.clear();
        strip.show();
        currentEffect = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// 蜂鸣器播放任务
void Buzzer_Task(void *pvParameters) {
  int songIndex = *(int*)pvParameters;
  Song song;
  memcpy_P(&song, &songs[songIndex], sizeof(Song));
  static int currentNoteIndex = 0; // Track current note index

  for (;;) {
    if (stopBuzzerTask) {
      noTone(BUZZER_PIN);
      strip.clear();
      strip.show();
      stopBuzzerTask = false;
      Serial.println("Buzzer_Task 被外部中断，已停止");
      firstDraw = true;
      currentNoteIndex = 0; // Reset note index
      break;
    }

    // Pause/Resume logic
    if (isPaused) {
      noTone(BUZZER_PIN); // Ensure buzzer is off during pause
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    for (int i = currentNoteIndex; i < song.length; i++) {
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        strip.clear();
        strip.show();
        stopBuzzerTask = false;
        firstDraw = true;
        currentNoteIndex = 0; // Reset note index
        goto exit_loop;
      }

// If paused, wait here without advancing the note index
      while (isPaused) {
        noTone(BUZZER_PIN); // Ensure buzzer is off during pause
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait before checking again
      }

      int note = pgm_read_word(song.melody+i);
      int duration = pgm_read_word(song.durations+i);
      
      tone(BUZZER_PIN, note, duration*0.9);
      
      currentLedCommand.effectType = (i % 2 == 0) ? 1 : 2;
      currentLedCommand.color = mapFrequencyToColor(note);
      currentLedCommand.frequency = note;

      // FFT processing
      for (int j = 0; j < SAMPLES; j++) {
        vReal[j] = sin(2 * PI * pgm_read_word(song.melody + i) * j / SAMPLING_FREQUENCY);
        vImag[j] = 0;
      }
      FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
      FFT.complexToMagnitude(vReal, vImag, SAMPLES);

      for (int j = 0; j < NUM_BANDS; j++) {
        spectrum[j] = 0;
        int start = j * (SAMPLES / 2 / NUM_BANDS);
        int end = (j + 1) * (SAMPLES / 2 / NUM_BANDS);
        for (int k = start; k < end; k++) {
          spectrum[j] += vReal[k];
        }
        spectrum[j] /= (end - start);
      }

      displayPlayingSong(songIndex, i, song.length, note, colorSchemes[song.colorSchemeIndex]);
      vTaskDelay(pdMS_TO_TICKS(duration));
      noTone(BUZZER_PIN);
      currentNoteIndex = i + 1; // Update note index after playing
    }

    Serial.printf("%s 播放完成\n", song.name);

    // 根据播放模式决定下一首
    if (currentPlayMode == SINGLE_LOOP) {
      currentNoteIndex = 0; // Reset for single loop
    } else if (currentPlayMode == LIST_LOOP) {
      songIndex = (songIndex + 1) % numSongs;
      memcpy_P(&song, &songs[songIndex], sizeof(Song));
      currentNoteIndex = 0; // Reset note index for new song
    } else if (currentPlayMode == RANDOM_PLAY) {
      songIndex = random(numSongs);
      memcpy_P(&song, &songs[songIndex], sizeof(Song));
      currentNoteIndex = 0; // Reset note index for new song
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
    firstDraw = true;
  }
exit_loop:
  strip.clear();
  strip.show();
  vTaskDelete(NULL);
}

void Buzzer_PlayMusic_Task(void *pvParameters) {
  int songIndex = *(int*)pvParameters;
  Song song;
  memcpy_P(&song, &songs[songIndex], sizeof(Song));

  Serial.printf("Starting background song: %s\n", song.name);

  for (int i = 0; i < song.length; i++) {
    if (stopBuzzerTask) {
      noTone(BUZZER_PIN);
      strip.clear();
      strip.show();
      stopBuzzerTask = false;
      Serial.println("Buzzer_PlayMusic_Task stopped by flag.");
      vTaskDelete(NULL);
      return;
    }

    int note = pgm_read_word(song.melody + i);
    int duration = pgm_read_word(song.durations + i);
    
    tone(BUZZER_PIN, note, duration*0.9);

    // Keep LED effects
    currentLedCommand.effectType = (i % 2 == 0) ? 1 : 2;
    currentLedCommand.color = mapFrequencyToColor(note);
    currentLedCommand.frequency = note;

    vTaskDelay(pdMS_TO_TICKS(duration));
    
    noTone(BUZZER_PIN); // Ensure tone is off before next note
  }

  Serial.printf("Background song %s finished.\n", song.name);
  
  strip.clear();
  strip.show();
  vTaskDelete(NULL);
}

// 双击检测
bool detectDoubleClick() {
  static unsigned long lastClickTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastClickTime < 500) {
    lastClickTime = 0;
    return true;
  }
  lastClickTime = currentTime;
  return false;
}

// 蜂鸣器菜单函数
void BuzzerMenu() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  initRotaryEncoder();
  strip.show();
  stopBuzzerTask = false;
  stopLedTask = false;
  selectedSongIndex = 0;
  displayOffset = 0;
  firstDraw = true;
  isPaused = false;

  // 歌曲选择循环
select_song:
  displaySongList(selectedSongIndex);

  while (1) {
    if (exitSubMenu) {
        exitSubMenu = false;
        stopLedTask = true;
        return;
    }
    
    int encoderChange = readEncoder();
    if (encoderChange != 0) {
      selectedSongIndex = (selectedSongIndex + encoderChange + numSongs) % numSongs;
      if (selectedSongIndex < displayOffset) {
        displayOffset = selectedSongIndex;
      } else if (selectedSongIndex >= displayOffset + visibleSongs) {
        displayOffset = selectedSongIndex - visibleSongs + 1;
      }
      displaySongList(selectedSongIndex);
      tone(BUZZER_PIN, 1000, 50);
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
        showMenuConfig();
        return;
      }
      Serial.printf("确认播放歌曲：%s\n", songs[selectedSongIndex].name);
      firstDraw = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // 启动播放任务
  xTaskCreatePinnedToCore(Buzzer_Task, "Buzzer_Task", 8192, &selectedSongIndex, 1, NULL, 0);
  xTaskCreatePinnedToCore(Led_Task, "Led_Task", 4096, NULL, 1, NULL, 0);

  // 播放控制循环
  while (1) {
    if (exitSubMenu) {
        exitSubMenu = false;
        stopBuzzerTask = true;
        stopLedTask = true;
        vTaskDelay(pdMS_TO_TICKS(150)); 
        return;
    }

    // 旋转编码器：切换播放模式
    int encoderChange = readEncoder();
    if (encoderChange != 0) {
      int mode = (int)currentPlayMode;
      mode = (mode + encoderChange) % 3;
      if (mode < 0) mode += 3;
      currentPlayMode = (PlayMode)mode;
      firstDraw = true;
      Serial.print("播放模式已切换为: ");
      switch (currentPlayMode) {
        case SINGLE_LOOP: Serial.println("单曲循环"); break;
        case LIST_LOOP: Serial.println("列表播放"); break;
        case RANDOM_PLAY: Serial.println("随机播放"); break;
      }
    }

    // 按钮：单击暂停/播放，双击返回歌曲选择
    if (readButton()) {
      if (detectDoubleClick()) {
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
        // display = 48;
        // picture_flag = 0;
        // showMenuConfig();
        return;
      } else {
        isPaused = !isPaused;
        if (isPaused) {
          Serial.println("音乐暂停");
          noTone(BUZZER_PIN);
          currentLedCommand.effectType = 3;
          currentLedCommand.color = strip.Color(0, 0, 0);
        } else {
          Serial.println("音乐继续");
        }
        tone(BUZZER_PIN, 1000, 50);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}