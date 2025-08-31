#include "Buzzer.h"
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include <time.h>
#include "LED.h"
#include "arduinoFFT.h"
#include <math.h>
#include "Menu.h"
// FFT Constants
#define SAMPLES 256
#define SAMPLING_FREQUENCY 4000
#define NUM_BANDS 16

// FFT Globals
ArduinoFFT<double> FFT = ArduinoFFT<double>();
double vReal[SAMPLES];
double vImag[SAMPLES];
double spectrum[NUM_BANDS];


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
  {"Da Hai", melody_da_hai, durations_da_hai, sizeof(melody_da_hai) / sizeof(melody_da_hai[0]), 1}, 
  {"Happy Birthday", melody_happy_birthday, durations_happy_birthday, sizeof(melody_happy_birthday) / sizeof(melody_happy_birthday[0]), 2}, 
  {"Windows XP",melody_windows,durations_windows,sizeof(melody_windows) / sizeof(melody_windows[0]),3
  },
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
float calculateSongDuration(const Song* song) {
  int totalMs = 0;
  for (int i = 0; i < song->length; i++) {
int duration = pgm_read_word(song->durations+i);  // 从 PROGMEM 读取
    totalMs += duration * 1.3;
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
  // time_t now;
  // time(&now);
  // char timeStr[20];
  // strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&now));
  // tft.print("Time: ");
  // tft.print(timeStr);

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

void drawSpectrum(const ColorScheme& scheme) {
  // Clear the previous spectrum, within the border
  tft.fillRect(5, 165, 230, 70, interpolateColor(scheme.bgColorStart, scheme.bgColorEnd, (float)165 / 239.0));

  int barWidth = 230 / NUM_BANDS; // Adjust width for the new area
  for (int i = 0; i < NUM_BANDS; i++) {
    int barHeight = spectrum[i] * 100; // Scale up for display (increased factor)
    if (barHeight > 70) { // Max height is now 70
      barHeight = 70;
    }
    uint16_t barColor = TFT_GREEN;
    // Draw bars from the bottom of the spectrum area (Y=235) upwards
    tft.fillRect(5 + i * barWidth, 235 - barHeight, barWidth - 2, barHeight, barColor);
  }
}

// 显示播放界面
void displayPlayingSong(int songIndex, int noteIndex, int totalNotes, int currentNote, const ColorScheme& scheme) {
  static int lastNoteIndex = -1;

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
    tft.println(songs[songIndex].name);

    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.printf("Duration: %.1f s", calculateSongDuration(&songs[songIndex]));

    // 进度条背景
    tft.fillRect(20, 130, 200, 10, TFT_DARKGREY); // Keep darkgrey for now, can be scheme color later

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
  Song song;
  memcpy_P(&song, &songs[songIndex], sizeof(Song));
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

    for (int i = 0; i < song.length; i++) {
      int note = pgm_read_word(song.melody+i);  // 读取音符
      int duration = pgm_read_word(song.durations+i);  // 读取时长
      if (stopBuzzerTask) {
        noTone(BUZZER_PIN);
        strip.clear(); // Clear LEDs on exit
        strip.show();  // Turn off LEDs
        stopBuzzerTask = false;
        firstDraw = true;
        goto exit_loop;
      }
      tone(BUZZER_PIN, note, duration);
      // Send command to Led_Task
      currentLedCommand.effectType = (i % 2 == 0) ? 1 : 2; // Alternate colorWipe and rainbow
      currentLedCommand.color = mapFrequencyToColor(note);
      currentLedCommand.frequency = note;

      // Synthesize audio and perform FFT
      for (int j = 0; j < SAMPLES; j++) {
        vReal[j] = sin(2 * PI * pgm_read_word(song.melody + i) * j / SAMPLING_FREQUENCY);
        vImag[j] = 0;
      }
      FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
      FFT.complexToMagnitude(vReal, vImag, SAMPLES);

      // Group FFT results into bands
      for (int j = 0; j < NUM_BANDS; j++) {
        spectrum[j] = 0;
        int start = j * (SAMPLES / 2 / NUM_BANDS);
        int end = (j + 1) * (SAMPLES / 2 / NUM_BANDS);
        for (int k = start; k < end; k++) {
          spectrum[j] += vReal[k];
        }
        spectrum[j] /= (end - start);
      }

      // DEBUG: Print spectrum data
      Serial.println("Spectrum data:");
      for (int j = 0; j < NUM_BANDS; j++) {
        Serial.print(spectrum[j]);
        Serial.print(" ");
      }
      Serial.println();
      
      displayPlayingSong(songIndex, i, song.length, note, colorSchemes[song.colorSchemeIndex]);
      vTaskDelay(pdMS_TO_TICKS(duration * 1.1));
      noTone(BUZZER_PIN);
      // Led_Task will handle clearing the strip after the effect
    }
    Serial.printf("%s 一轮播放完成\n", song.name);
    vTaskDelay(pdMS_TO_TICKS(2000));
    firstDraw = true; // 重置绘制标志
  }
exit_loop:
  strip.clear(); // Ensure LEDs are off when task deletes itself
  strip.show();
  vTaskDelete(NULL);
}

// New task for playing music in the background without screen updates
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
    
    tone(BUZZER_PIN, note, duration);

    // Keep LED effects
    currentLedCommand.effectType = (i % 2 == 0) ? 1 : 2;
    currentLedCommand.color = mapFrequencyToColor(note);
    currentLedCommand.frequency = note;

    vTaskDelay(pdMS_TO_TICKS(duration * 1.1));
    
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
        // display = 48;
        // picture_flag = 0;
        // showMenuConfig();
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
