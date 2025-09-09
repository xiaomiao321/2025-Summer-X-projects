#include "Buzzer.h"
#include "Alarm.h"
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

// --- Song Data ---
const Song songs[] PROGMEM= {
  { "Bao Wei Huang He", melody_bao_wei_huang_he, durations_bao_wei_huang_he, sizeof(melody_bao_wei_huang_he) / sizeof(melody_bao_wei_huang_he[0]), 0 },
  { "Bu Zai You Yu", melody_bu_zai_you_yu, durations_bu_zai_you_yu, sizeof(melody_bu_zai_you_yu) / sizeof(melody_bu_zai_you_yu[0]), 0 },
  { "Cai Bu Tou", melody_cai_bu_tou, durations_cai_bu_tou, sizeof(melody_cai_bu_tou) / sizeof(melody_cai_bu_tou[0]), 0 },
  { "Casablanca", melody_casablanca, durations_casablanca, sizeof(melody_casablanca)/sizeof(melody_casablanca[0]), 2 },
  { "Cheng Du", melody_cheng_du, durations_cheng_du, sizeof(melody_cheng_du)/sizeof(melody_cheng_du[0]), 2 },
  { "Chun Jiao Yu Zhi Ming",melody_chun_jiao_yu_zhi_ming,durations_chun_jiao_yu_zhi_ming,sizeof(melody_chun_jiao_yu_zhi_ming)/sizeof(melody_chun_jiao_yu_zhi_ming[0]),1},
  { "Da Hai", melody_da_hai, durations_da_hai, sizeof(melody_da_hai)/sizeof(melody_da_hai[0]), 2 },
  { "Dong Fang Zhi Zhu", melody_dong_fang_zhi_zhu, durations_dong_fang_zhi_zhu, sizeof(melody_dong_fang_zhi_zhu)/sizeof(melody_dong_fang_zhi_zhu[0]), 2 },
  { "Dream Wedding", melody_dream_wedding, durations_dream_wedding, sizeof(melody_dream_wedding)/sizeof(melody_dream_wedding[0]), 2 },
  { "Fan Fang Xiang De Zhong", melody_fan_fang_xiang_de_zhong, durations_fan_fang_xiang_de_zhong, sizeof(melody_fan_fang_xiang_de_zhong)/sizeof(melody_fan_fang_xiang_de_zhong[0]), 2 },
  { "For Elise", melody_for_elise, durations_for_elise, sizeof(melody_for_elise)/sizeof(melody_for_elise[0]), 2 },
  { "Ge Chang Zu Guo", melody_ge_chang_zu_guo, durations_ge_chang_zu_guo, sizeof(melody_ge_chang_zu_guo)/sizeof(melody_ge_chang_zu_guo[0]), 2 },
  { "Guo Ji Ge", melody_guo_ji_ge, durations_guo_ji_ge, sizeof(melody_guo_ji_ge)/sizeof(melody_guo_ji_ge[0]), 2 },
  {"Hai Kuo Tian Kong",melody_hai_kuo_tian_kong,durations_hai_kuo_tian_kong, sizeof(melody_hai_kuo_tian_kong)/sizeof(melody_hai_kuo_tian_kong[0]), 3 },
  { "Hong Dou", melody_hong_dou, durations_hong_dou, sizeof(melody_hong_dou)/sizeof(melody_hong_dou[0]), 4 },
  { "Hong Se Gao Gen Xie", melody_hong_se_gao_gen_xie, durations_hong_se_gao_gen_xie, sizeof(melody_hong_se_gao_gen_xie)/sizeof(melody_hong_se_gao_gen_xie[0]), 0 },
  { "Hou Lai", melody_hou_lai, durations_hou_lai, sizeof(melody_hou_lai)/sizeof(melody_hou_lai[0]), 0 },
  { "Kai Shi Dong Le", melody_kai_shi_dong_le, durations_kai_shi_dong_le, sizeof(melody_kai_shi_dong_le)/sizeof(melody_kai_shi_dong_le[0]), 1 },
  { "Lan Ting Xu", melody_lan_ting_xu, durations_lan_ting_xu, sizeof(melody_lan_ting_xu)/sizeof(melody_lan_ting_xu[0]), 2 },
  { "Liang Zhu", melody_liang_zhu, durations_liang_zhu, sizeof(melody_liang_zhu)/sizeof(melody_liang_zhu[0]), 2 },
  { "Lv Se", melody_lv_se, durations_lv_se, sizeof(melody_lv_se)/sizeof(melody_lv_se[0]), 2 },
  { "Mi Ren De Wei Xian",melody_mi_ren_de_wei_xian,durations_mi_ren_de_wei_xian,sizeof(melody_mi_ren_de_wei_xian)/sizeof(melody_mi_ren_de_wei_xian[0]),3},
  { "Qi Feng Le", melody_qi_feng_le, durations_qi_feng_le, sizeof(melody_qi_feng_le)/sizeof(melody_qi_feng_le[0]), 3 },
  { "Qing Hua Ci", melody_qing_hua_ci, durations_qing_hua_ci, sizeof(melody_qing_hua_ci)/sizeof(melody_qing_hua_ci[0]), 3 },
  { "Tong Nian", melody_tong_nian, durations_tong_nian, sizeof(melody_tong_nian)/sizeof(melody_tong_nian[0]), 2 },
  { "Turkish March", melody_turkish_march, durations_turkish_march, sizeof(melody_turkish_march)/sizeof(melody_turkish_march[0]), 2 },
  { "Xiao Xiao Niao", melody_wo_shi_yi_zhi_xiao_xiao_niao, durations_wo_shi_yi_zhi_xiao_xiao_niao,sizeof(melody_wo_shi_yi_zhi_xiao_xiao_niao)/sizeof(melody_wo_shi_yi_zhi_xiao_xiao_niao[0]), 4 },
  { "Xi Huan Ni", melody_xi_huan_ni, durations_xi_huan_ni,sizeof(melody_xi_huan_ni)/sizeof(melody_xi_huan_ni[0]), 4 },
  { "Xin Qiang", melody_xin_qiang, durations_xin_qiang,sizeof(melody_xin_qiang)/sizeof(melody_xin_qiang[0]), 4 },
  { "Ye Feng Fei Wu", melody_ye_feng_fei_wu, durations_ye_feng_fei_wu, sizeof(melody_ye_feng_fei_wu)/sizeof(melody_ye_feng_fei_wu[0]), 0 },
  { "Yi Sheng You Ni", melody_yi_sheng_you_ni, durations_yi_sheng_you_ni, sizeof(melody_yi_sheng_you_ni)/sizeof(melody_yi_sheng_you_ni[0]), 0 },
  { "You Dian Tian", melody_you_dian_tian, durations_you_dian_tian, sizeof(melody_you_dian_tian)/sizeof(melody_you_dian_tian[0]), 0 },
  { "Yu Jian", melody_yu_jian, durations_yu_jian, sizeof(melody_yu_jian)/sizeof(melody_yu_jian[0]), 0 },
  { "Zhen De Ai Ni",melody_zhen_de_ai_ni,durations_zhen_de_ai_ni,sizeof(melody_zhen_de_ai_ni) / sizeof(melody_zhen_de_ai_ni[0]),3},
  { "Windows XP",melody_windows,durations_windows,sizeof(melody_windows) / sizeof(melody_windows[0]),3},
};
const int numSongs = sizeof(songs) / sizeof(songs[0]);

// --- UI State ---
int selectedSongIndex = 0;
int displayOffset = 0;
const int visibleSongs = 3;

// --- Forward Declarations ---
static void stop_buzzer_playback();

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

    // Display Play/Pause status
    String status_text = isPaused ? "Paused" : "Playing";
    menuSprite.drawString(status_text, 120, 80);

    // Display Play Mode below status
    String mode_text;
    switch (currentPlayMode) {
        case SINGLE_LOOP: mode_text = "Single Loop"; break;
        case LIST_LOOP:   mode_text = "List Loop"; break;
        case RANDOM_PLAY: mode_text = "Random"; break;
    }
    menuSprite.setTextSize(1); // Use smaller text for the mode
    menuSprite.drawString(mode_text, 120, 100);
    menuSprite.setTextSize(2); // Reset text size

    // --- Time-domain song visualization ---
    Song current_song;
    memcpy_P(&current_song, &songs[shared_song_index], sizeof(Song));

    if (current_song.length > 0) {
        // Find min (non-zero) and max frequency for scaling
        int min_freq = 10000;
        int max_freq = 0;
        for (int i = 0; i < current_song.length; i++) {
            int freq = pgm_read_word(current_song.melody + i);
            if (freq > 0) {
                if (freq < min_freq) min_freq = freq;
                if (freq > max_freq) max_freq = freq;
            }
        }
        // Handle songs with no audible notes or a single note
        if (min_freq > max_freq) { min_freq = 200; max_freq = 2000; }
        if (min_freq == max_freq) { min_freq = max_freq / 2; }

        // Drawing parameters
        const int graph_x = 10;
        const int graph_y_bottom = 180;
        const int graph_area_width = 220;
        const int max_bar_height = 75; // Increased from 60
        const int min_bar_height = 2;

        float step = (float)graph_area_width / current_song.length;
        int bar_width = (step > 1.0f) ? floor(step * 0.8f) : 1; // 80% of space, or 1px minimum
        if (bar_width == 0) bar_width = 1;

        for (int i = 0; i < current_song.length; i++) {
            int freq = pgm_read_word(current_song.melody + i);
            int bar_height = 0;
            if (freq > 0) {
                bar_height = map(freq, min_freq, max_freq, min_bar_height, max_bar_height);
            }

            if (bar_height > 0) {
                uint16_t color = (i <= shared_note_index) ? TFT_CYAN : TFT_DARKGREY;
                int x_pos = graph_x + floor(i * step);
                menuSprite.fillRect(x_pos, graph_y_bottom - bar_height, bar_width, bar_height, color);
            }
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

    // --- Current Note Info ---
    int current_freq = 0;
    int current_dur = 0;
    if (!isPaused && shared_note_index < current_song.length) {
        current_freq = pgm_read_word(current_song.melody + shared_note_index);
        current_dur = pgm_read_word(current_song.durations + shared_note_index);
    }
    char note_info[30];
    snprintf(note_info, sizeof(note_info), "Note: %d Hz, %d ms", current_freq, current_dur);
    menuSprite.setTextSize(1); // Use smaller text
    menuSprite.drawString(note_info, 120, 228);

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
        noTone(BUZZER_PIN);
        current_note_start_tick = xTaskGetTickCount();
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      int note = pgm_read_word(song.melody+i);
      int duration = pgm_read_word(song.durations+i);
      if (note > 0) {
        tone(BUZZER_PIN, note, duration);
      }
      vTaskDelay(pdMS_TO_TICKS(duration));
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
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
        tone(BUZZER_PIN, note, duration);
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
