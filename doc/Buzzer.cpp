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
