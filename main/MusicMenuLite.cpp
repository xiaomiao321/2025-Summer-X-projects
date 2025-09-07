#include "MusicMenuLite.h"
#include "Alarm.h"
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
                tone(BUZZER_PIN, note, duration);
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
            isPaused = !isPaused;
            tone(BUZZER_PIN, 1000, 50);
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