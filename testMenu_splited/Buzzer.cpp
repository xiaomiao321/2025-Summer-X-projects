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
#include "Alarm.h"
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
    totalMs += duration;
  }
  return totalMs / 1000.0;
}

// 显示歌曲选择列表
void displaySongList(int selectedIndex) {
  // 始终使用黑色背景
  menuSprite.fillScreen(TFT_BLACK);
  
  // 绘制标题
  menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  menuSprite.setTextSize(2);
  menuSprite.setTextDatum(MC_DATUM);
  menuSprite.drawString("Music Menu", 120, 28);
  
  // 绘制歌曲列表
  menuSprite.setTextWrap(true);
  for (int i = 0; i < visibleSongs; i++) {
    int songIdx = displayOffset + i;
    if (songIdx >= numSongs) break;
   
    int yPos = 60 + i * 50;
    
    // 选中的歌曲高亮显示
    if (songIdx == selectedIndex) {
      // 深蓝色背景
      menuSprite.fillRoundRect(10, yPos - 18, 220, 36, 5, 0x001F); 
      
      // 歌曲名称 - 白色文字
      menuSprite.setTextSize(2);
      menuSprite.setTextColor(TFT_WHITE, 0x001F);
      menuSprite.setTextDatum(MC_DATUM);
      menuSprite.drawString(songs[songIdx].name, 120, yPos);
    } else {
      // 未选中的歌曲 - 黑色背景
      menuSprite.fillRoundRect(10, yPos - 18, 220, 36, 5, TFT_BLACK);
      
      // 歌曲名称 - 白色文字
      menuSprite.setTextSize(1);
      menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
      menuSprite.setTextDatum(MC_DATUM);
      menuSprite.drawString(songs[songIdx].name, 120, yPos);
    }
  }
  
  // 重置文本对齐方式
  menuSprite.setTextDatum(TL_DATUM);
  menuSprite.pushSprite(0, 0);
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

// Helper to format time in MM:SS
void formatTime(char* buf, int totalSeconds) {
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  sprintf(buf, "%02d:%02d", minutes, seconds);
}

// 显示播放界面
void displayPlayingSong(int songIndex, int noteIndex, int totalNotes, int currentNote, int noteDuration, float elapsedTime) {
    static PlayMode lastPlayMode = (PlayMode)-1;
    static char lastTimeStr[20] = "";
    static int lastNoteIndex = -1;
    static float lastElapsedTime = -1.0;
    static int lastNoteDuration = -1; // Track last note duration for clearing
    static int lastCurrentNote = -1; // Track last current note for clearing

    if (firstDraw) {
        menuSprite.fillScreen(TFT_BLACK);
        
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextSize(2);
        menuSprite.setTextDatum(MC_DATUM);
        menuSprite.drawString(songs[songIndex].name, 120, 30); // Song Name
        menuSprite.setTextDatum(TL_DATUM);

        // Initial draw for note time/freq, play mode, clock time, etc.
        // These will be updated in their respective blocks below.
        firstDraw = false;
        lastPlayMode = (PlayMode)-1;
        strcpy(lastTimeStr, "");
        lastNoteIndex = -1;
        lastElapsedTime = -1.0;
        lastNoteDuration = -1;
        lastCurrentNote = -1;
    }

    // --- Display Current Note Time and Frequency ---
    // Only update if note or duration changes, or if it's the first draw
    if (noteDuration != lastNoteDuration || currentNote != lastCurrentNote || firstDraw) {
        menuSprite.fillRect(0, 45, 240, 30, TFT_BLACK); // Clear area for note info
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextSize(2); // Smaller size for note info
        menuSprite.setTextDatum(MC_DATUM);
        if (noteDuration > 0) { // Only display if a note is playing
            char noteInfoStr[50];
            sprintf(noteInfoStr, "%d Hz  %d ms", currentNote, noteDuration);
            menuSprite.drawString(noteInfoStr, 120, 57); 
        } else {
            menuSprite.drawString("Paused", 120, 55); // Display "Paused" when noteDuration is 0
        }
        menuSprite.setTextDatum(TL_DATUM);
        lastNoteDuration = noteDuration;
        lastCurrentNote = currentNote;
    }

    // --- Play Mode Update ---
    if(lastPlayMode != currentPlayMode){
        menuSprite.fillRect(0, 75, 240, 30, TFT_BLACK); // Clear area for play mode (moved down)
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextSize(2);
        menuSprite.setTextDatum(MC_DATUM);
        switch (currentPlayMode) {
            case SINGLE_LOOP: menuSprite.drawString("Single Loop", 120, 85); break; // Moved to y=85
            case LIST_LOOP: menuSprite.drawString("List Loop", 120, 85); break;
            case RANDOM_PLAY: menuSprite.drawString("Random", 120, 85); break;
        }
        menuSprite.setTextDatum(TL_DATUM);
        lastPlayMode = currentPlayMode;
    }

    // --- Current Clock Time Update ---
    if (!getLocalTime(&timeinfo)) { return; }
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    if (strcmp(timeStr, lastTimeStr) != 0) {
        menuSprite.fillRect(0, 105, 240, 30, TFT_BLACK); // Clear area for clock time (moved down)
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextSize(2);
        menuSprite.setTextDatum(MC_DATUM);
        menuSprite.drawString(timeStr, 120, 115); // Moved to y=115
        menuSprite.setTextDatum(TL_DATUM);
        strcpy(lastTimeStr, timeStr);
    }

    // --- Note Count and Elapsed/Total Time Update ---
    // Only update if noteIndex or elapsedTime changes
    if (noteIndex != lastNoteIndex || (int)elapsedTime != (int)lastElapsedTime) {
        // Clear previous time and note count area
        menuSprite.fillRect(0, 130, 240, 20, TFT_BLACK); // Clear area for time/note count (moved down)

        char totalTimeStr[6];
        formatTime(totalTimeStr, (int)calculateSongDuration(&songs[songIndex]));
        char elapsedTimeStr[6];
        formatTime(elapsedTimeStr, (int)elapsedTime);

        menuSprite.setTextSize(2); // Changed to size 2
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextDatum(TL_DATUM);
        menuSprite.drawString(elapsedTimeStr, 10, 140); // Moved to y=140
        menuSprite.setTextDatum(TR_DATUM); // Align total time to right
        menuSprite.drawString(totalTimeStr, 230, 140); // Moved to y=140
        menuSprite.setTextDatum(TL_DATUM); // Reset datum

        char noteCountStr[20];
        sprintf(noteCountStr, "%d/%d", noteIndex + 1, totalNotes);
        menuSprite.setTextSize(2); // Changed to size 2
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextDatum(MC_DATUM);
        // menuSprite.drawString(noteCountStr, 120, 140); // Centered at y=140
        menuSprite.setTextDatum(TL_DATUM); // Reset datum
        
        lastElapsedTime = elapsedTime;
        lastNoteIndex = noteIndex;
    }

    // --- Progress Bar ---
    // Always redraw to ensure smooth update
    menuSprite.fillRect(10, 155, 220, 10, TFT_BLACK); // Clear previous progress bar (moved down)
    int progressWidth = (noteIndex + 1) * 220 / totalNotes;
    menuSprite.fillRoundRect(10, 155, progressWidth, 10, 5, TFT_WHITE); // Moved to y=155

    // --- Spectrum analyzer ---
    menuSprite.fillRect(5, 180, 230, 70, TFT_BLACK); // Clear previous spectrum (moved down)
    int barWidth = 8;
    int barSpacing = 14;
    // uint16_t deepBlue = TFT_CYAN; // Unused variable
    for (int i = 0; i < NUM_BANDS; i++) {
        int barHeight = spectrum[i] * 100;
        if (barHeight > 70) barHeight = 70;
        if (barHeight < 5) barHeight = 0; // Don't draw tiny bars

        if(barHeight > 0) {
            uint16_t barColor = TFT_CYAN;
            menuSprite.fillRoundRect(10 + i * barSpacing, 250 - barHeight, barWidth, barHeight, 4, barColor);
        }
    }
    menuSprite.pushSprite(0, 0);
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
      stopLedTask = false; // Reset flag
      vTaskDelete(NULL); // Self-delete
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
  static int lastDisplayedNoteIndex = 0; // Added for pause display
  static float lastDisplayedElapsedTime = 0.0; // Added for pause display

  for (;;) {
    if (stopBuzzerTask) {
      noTone(BUZZER_PIN);
      strip.clear();
      strip.show();
      Serial.println("Buzzer_Task 被外部中断，已停止");
      firstDraw = true;
      currentNoteIndex = 0; // Reset note index
      lastDisplayedNoteIndex = 0; // Reset on exit
      lastDisplayedElapsedTime = 0.0; // Reset on exit
      vTaskDelay(pdMS_TO_TICKS(50)); // Small delay before deleting
      vTaskDelete(NULL); // Self-delete
    }

    // Pause/Resume logic
    if (isPaused) {
      noTone(BUZZER_PIN); // Ensure buzzer is off during pause
      // Removed vTaskDelay and continue, handled by the while(isPaused) loop below
    }

    for (int i = currentNoteIndex; i < song.length; i++) {
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        strip.clear();
        strip.show();
        firstDraw = true;
        currentNoteIndex = 0; // Reset note index
        goto exit_loop;
      }

// If paused, wait here without advancing the note index
      while (isPaused) {
        noTone(BUZZER_PIN); // Ensure buzzer is off during pause
        if (stopBuzzerTask) {
            noTone(BUZZER_PIN);
            strip.clear();
            strip.show();
            firstDraw = true;
            currentNoteIndex = 0; // Reset note index
            lastDisplayedNoteIndex = 0; // Reset on exit
            lastDisplayedElapsedTime = 0.0; // Reset on exit
            goto exit_loop;
        }
        displayPlayingSong(songIndex, lastDisplayedNoteIndex, song.length, 0, 0, lastDisplayedElapsedTime); // Update display (0 for noteDuration when paused)
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

      // Calculate elapsed time
      float elapsedTime = 0;
      for (int j = 0; j <= i; j++) { // <= i because i is the current note index
          elapsedTime += pgm_read_word(song.durations+j);
      }
      elapsedTime /= 1000.0; // convert to seconds

      // displayPlayingSong(songIndex, i, song.length, note, elapsedTime); // Initial call removed
      
      unsigned long noteStartTime = millis();
      while (millis() - noteStartTime < duration) {
        if (stopBuzzerTask) {
          noTone(BUZZER_PIN);
          strip.clear();
          strip.show();
          firstDraw = true;
          currentNoteIndex = 0; // Reset note index
          lastDisplayedNoteIndex = 0; // Reset on exit
          lastDisplayedElapsedTime = 0.0; // Reset on exit
          goto exit_loop;
        }
        // Recalculate elapsedTime for song progress
        float currentElapsedTime = 0;
        for (int j = 0; j < i; j++) { // Sum durations of notes already played
            currentElapsedTime += pgm_read_word(song.durations+j);
        }
        currentElapsedTime += (millis() - noteStartTime); // Add elapsed time of current note
        currentElapsedTime /= 1000.0; // convert to seconds

        lastDisplayedNoteIndex = i; // Update last displayed values
        lastDisplayedElapsedTime = currentElapsedTime;

        displayPlayingSong(songIndex, i, song.length, note, duration, currentElapsedTime); // Call periodically
        vTaskDelay(pdMS_TO_TICKS(10));
      }

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

    unsigned long noteStartTime = millis();
    while(millis() - noteStartTime < duration)
    {
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        strip.clear();
        strip.show();
        stopBuzzerTask = false;
        Serial.println("Buzzer_PlayMusic_Task stopped by flag.");
        vTaskDelete(NULL);
        return;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    noTone(BUZZER_PIN); // Ensure tone is off before next note
  }

  Serial.printf("Background song %s finished.\n", song.name);
  
  strip.clear();
  strip.show();
  vTaskDelete(NULL);
}

// 双击检测
bool detectDoubleClick(bool reset = false) {
  static unsigned long lastClickTime = 0;
  if (reset) {
      lastClickTime = 0;
      return false; // Not a double click, just reset
  }
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

  // detectDoubleClick(true); // Reset double click state on entry

  // 歌曲选择循环
select_song:
  displaySongList(selectedSongIndex);

  while (1) {
    if (exitSubMenu || g_alarm_is_ringing) {
        exitSubMenu = false;
        stopLedTask = true;
        menuSprite.fillScreen(TFT_BLACK);
        menuSprite.pushSprite(0, 0);
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
      // if (detectDoubleClick()) { // 双击返回主菜单
      //   Serial.println("双击检测到，返回主菜单");
      //   stopLedTask = true; // Signal Led_Task to stop
      //   TaskHandle_t taskHandle = xTaskGetHandle("Led_Task");
      //   if (taskHandle != NULL) {
      //     vTaskDelete(taskHandle); // Delete Led_Task
      //   }
      //   showMenuConfig();
      //   return;
      // }
      Serial.printf("确认播放歌曲：%s\n", songs[selectedSongIndex].name);
      firstDraw = true;
      break;
    }
    if (readButtonLongPress()) {
      stopLedTask = true;
      // Led_Task will self-delete
      menuSprite.fillScreen(TFT_BLACK);
      menuSprite.pushSprite(0, 0);
      return;
  }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // 启动播放任务
  xTaskCreatePinnedToCore(Buzzer_Task, "Buzzer_Task", 8192, &selectedSongIndex, 1, NULL, 0);
  xTaskCreatePinnedToCore(Led_Task, "Led_Task", 4096, NULL, 1, NULL, 0);

  // 播放控制循环
  while (1) {
    if (exitSubMenu || g_alarm_is_ringing) {
        exitSubMenu = false;
        stopBuzzerTask = true;
        stopLedTask = true;
        vTaskDelay(pdMS_TO_TICKS(150)); // Give tasks time to stop
        TaskHandle_t buzzerTaskHandle = xTaskGetHandle("Buzzer_Task"); // Wait for Buzzer_Task
        if (buzzerTaskHandle != NULL) {
          const TickType_t xTimeout = pdMS_TO_TICKS(100);
          while (eTaskGetState(buzzerTaskHandle) != eDeleted && xTimeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        TaskHandle_t ledTaskHandle = xTaskGetHandle("Led_Task"); // Wait for Led_Task
        if (ledTaskHandle != NULL) {
          const TickType_t xTimeout = pdMS_TO_TICKS(100);
          while (eTaskGetState(ledTaskHandle) != eDeleted && xTimeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
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
    if(readButtonLongPress())
    {
        stopBuzzerTask = true;
        stopLedTask = true; // Signal Led_Task to stop
        TaskHandle_t buzzerTaskHandle = xTaskGetHandle("Buzzer_Task");
        if (buzzerTaskHandle != NULL) {
          const TickType_t xTimeout = pdMS_TO_TICKS(100);
          while (eTaskGetState(buzzerTaskHandle) != eDeleted && xTimeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        TaskHandle_t ledTaskHandle = xTaskGetHandle("Led_Task"); // Get handle for Led_Task
        if (ledTaskHandle != NULL) {
          const TickType_t xTimeout = pdMS_TO_TICKS(100); // Use same timeout
          while (eTaskGetState(ledTaskHandle) != eDeleted && xTimeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        // Led_Task will self-delete
        return;
    }
    // 按钮：单击暂停/播放，双击返回歌曲选择
    if (readButton()) {
      // if (detectDoubleClick()) {
      //   Serial.println("双击检测到，返回主菜单");
      //   stopBuzzerTask = true;
      //   stopLedTask = true; // Signal Led_Task to stop
      //   TaskHandle_t buzzerTaskHandle = xTaskGetHandle("Buzzer_Task");
      //   if (buzzerTaskHandle != NULL) {
      //     const TickType_t xTimeout = pdMS_TO_TICKS(100);
      //     while (eTaskGetState(buzzerTaskHandle) != eDeleted && xTimeout > 0) {
      //       vTaskDelay(pdMS_TO_TICKS(10));
      //     }
      //   }
      //   TaskHandle_t ledTaskHandle = xTaskGetHandle("Led_Task");
      //   if (ledTaskHandle != NULL) {
      //     vTaskDelete(ledTaskHandle); // Delete Led_Task
      //   }
        
      //   return;
      // } else {
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
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}