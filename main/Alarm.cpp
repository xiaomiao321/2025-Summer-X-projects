#include "Alarm.h"
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Buzzer.h"
#include "weather.h" // For time functions
#include "MQTT.h"    // For exitSubMenu
#include <EEPROM.h>
#include <freertos/task.h> // For task management
#include <pgmspace.h>

#define MAX_ALARMS 10
#define EEPROM_START_ADDR 0
#define EEPROM_MAGIC_KEY 0xAD
#define ALARMS_PER_PAGE 5

// --- Global state for alarm ringing ---
volatile bool g_alarm_is_ringing = false;

// --- Bitmasks for Days of the Week ---

enum EditMode { EDIT_HOUR, EDIT_MINUTE, EDIT_DAYS, EDIT_SAVE, EDIT_DELETE };

// --- Module-internal State Variables ---
static AlarmSetting alarms[MAX_ALARMS];
static int alarm_count = 0;
static int last_checked_day = -1;

// --- Task Handles ---
static TaskHandle_t alarmMusicTaskHandle = NULL;
static volatile bool stopAlarmMusic = false;

// --- UI State Variables ---
static int list_selected_index = 0;
static int list_scroll_offset = 0;
static unsigned long last_click_time = 0;
static const unsigned long DOUBLE_CLICK_WINDOW = 350; // ms

// =====================================================================================
//                                 FORWARD DECLARATIONS
// =====================================================================================
static void saveAlarms();
static void Alarm_Delete(int index);
static void editAlarm(int index);
static void drawAlarmList();
void Alarm_Loop_Check();


// =====================================================================================
//                                     DRAWING LOGIC
// =====================================================================================

static void drawAlarmList() {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextFont(1);
    menuSprite.setTextSize(2);
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.setTextColor(TFT_WHITE);

    extern struct tm timeinfo;
    char titleBuf[30]; // Already 30, which is sufficient
    strftime(titleBuf, sizeof(titleBuf), "%Y-%m-%d %H:%M:%S %a", &timeinfo); // New format
    menuSprite.drawString(titleBuf, 10, 10);
    
    if (alarm_count > ALARMS_PER_PAGE) {
        char scrollIndicator[10];
        int currentPage = list_scroll_offset / ALARMS_PER_PAGE + 1;
        int totalPages = (alarm_count + ALARMS_PER_PAGE - 1) / ALARMS_PER_PAGE;
        snprintf(scrollIndicator, sizeof(scrollIndicator), "%d/%d", currentPage, totalPages);
        menuSprite.drawString(scrollIndicator, 200, 10);
    }
    
    menuSprite.drawFastHLine(0, 32, 240, TFT_DARKGREY);

    int start_index = list_scroll_offset;
    int end_index = min(list_scroll_offset + ALARMS_PER_PAGE, alarm_count + 1);
    
    for (int i = start_index; i < end_index; i++) {
        int display_index = i - list_scroll_offset;
        int y_pos = 45 + (display_index * 38);
        
        if (i == list_selected_index) {
            menuSprite.drawRoundRect(5, y_pos - 5, 230, 36, 5, TFT_YELLOW);
        }

        if (i < alarm_count) {
            int box_x = 15, box_y = y_pos - 2;
            menuSprite.drawRect(box_x, box_y, 20, 20, TFT_WHITE);
            if (alarms[i].enabled) {
                menuSprite.drawLine(box_x + 4, box_y + 10, box_x + 8, box_y + 14, TFT_GREEN);
                menuSprite.drawLine(box_x + 8, box_y + 14, box_x + 16, box_y + 6, TFT_GREEN);
            } else {
                menuSprite.drawLine(box_x + 4, box_y + 4, box_x + 16, box_y + 16, TFT_RED);
                menuSprite.drawLine(box_x + 16, box_y + 4, box_x + 4, box_y + 16, TFT_RED);
            }

            char buf[20];
            snprintf(buf, sizeof(buf), "%02d:%02d", alarms[i].hour, alarms[i].minute);
            menuSprite.drawString(buf, 50, y_pos);

            const char* days[] = {"S", "M", "T", "W", "T", "F", "S"};
            for(int d=0; d<7; d++){
                menuSprite.setTextColor((alarms[i].days_of_week & (1 << d)) ? TFT_GREEN : TFT_DARKGREY);
                menuSprite.drawString(days[d], 140 + (d * 12), y_pos);
            }
            menuSprite.setTextColor(TFT_WHITE);
        } else if (i == alarm_count && i < MAX_ALARMS) {
            menuSprite.drawString("+ Add New Alarm", 15, y_pos);
        }
    }
    menuSprite.pushSprite(0, 0);
}

static void drawEditScreen(const AlarmSetting& alarm, EditMode mode, int day_cursor) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);

    menuSprite.setTextFont(7); menuSprite.setTextSize(1);
    char time_buf[6];
    sprintf(time_buf, "%02d:%02d", alarm.hour, alarm.minute);
    menuSprite.drawString(time_buf, 120, 80);

    if (mode == EDIT_HOUR) menuSprite.fillRect(38, 115, 70, 4, TFT_YELLOW);
    else if (mode == EDIT_MINUTE) menuSprite.fillRect(132, 115, 70, 4, TFT_YELLOW);

    menuSprite.setTextFont(1);
    const char* days[] = {"S","M", "T", "W", "T", "F", "S"};
    for(int d=0; d<7; d++){
        int day_x = 24 + (d * 30); int day_y = 160;
        if (mode == EDIT_DAYS && d == day_cursor) menuSprite.drawRect(day_x - 10, day_y - 12, 20, 24, TFT_YELLOW);
        menuSprite.setTextColor((alarm.days_of_week & (1 << d)) ? TFT_GREEN : TFT_DARKGREY);
        menuSprite.drawString(days[d], day_x, day_y);
    }

    int save_box_y = 205;
    menuSprite.setTextFont(1); menuSprite.setTextColor(TFT_WHITE);
    if (mode == EDIT_SAVE) {
        menuSprite.fillRoundRect(40, save_box_y, 75, 30, 5, TFT_GREEN);
        menuSprite.setTextColor(TFT_BLACK);
    }
    menuSprite.drawRoundRect(40, save_box_y, 75, 30, 5, TFT_WHITE);
    menuSprite.drawString("SAVE", 78, save_box_y + 15);

    menuSprite.setTextColor(TFT_WHITE);
    if (mode == EDIT_DELETE) {
        menuSprite.fillRoundRect(125, save_box_y, 75, 30, 5, TFT_RED);
        menuSprite.setTextColor(TFT_BLACK);
    }
    menuSprite.drawRoundRect(125, save_box_y, 75, 30, 5, TFT_WHITE);
    menuSprite.drawString("DELETE", 163, save_box_y + 15);

    menuSprite.pushSprite(0, 0);
}

// =====================================================================================
//                                 BACKGROUND LOGIC
// =====================================================================================

static void saveAlarms() {
  EEPROM.write(EEPROM_START_ADDR, EEPROM_MAGIC_KEY);
  EEPROM.put(EEPROM_START_ADDR + 1, alarms);
  EEPROM.commit();
}

static void loadAlarms() {
  if (EEPROM.read(EEPROM_START_ADDR) == EEPROM_MAGIC_KEY) {
    EEPROM.get(EEPROM_START_ADDR + 1, alarms);
    alarm_count = 0;
    for (int i = 0; i < MAX_ALARMS; ++i) if (alarms[i].hour != 255) alarm_count++;
  } else {
    memset(alarms, 0xFF, sizeof(alarms));
    for(int i=0; i<MAX_ALARMS; ++i) alarms[i].enabled = false;
    alarm_count = 0;
    saveAlarms();
  }
}

static void Alarm_Delete(int index) {
    if (index < 0 || index >= alarm_count) return;
    for (int i = index; i < alarm_count - 1; i++) {
        alarms[i] = alarms[i + 1];
    }
    alarm_count--;
    memset(&alarms[alarm_count], 0xFF, sizeof(AlarmSetting));
    alarms[alarm_count].enabled = false;
    saveAlarms();
}

void Alarm_MusicLoop_Task(void *pvParameters) {
    while (true) {
        for (int i = 0; i < numSongs; i++) {
            if (stopAlarmMusic) {
                vTaskDelete(NULL);
            }

            Song currentSong;
            memcpy_P(&currentSong, &songs[i], sizeof(Song));

            for (int j = 0; j < currentSong.length; j++) {
                if (stopAlarmMusic) {
                    noTone(BUZZER_PIN);
                    vTaskDelete(NULL);
                }
                int note = pgm_read_word(&currentSong.melody[j]);
                int duration = pgm_read_word(&currentSong.durations[j]);
                
                tone(BUZZER_PIN, note, duration * 0.9);
                vTaskDelay(pdMS_TO_TICKS(duration));
            }
            vTaskDelay(pdMS_TO_TICKS(500)); // Pause between songs
        }
    }
}

static void triggerAlarm(int index) {
  alarms[index].triggered_today = true;
  saveAlarms();
  Serial.printf("ALARM %d TRIGGERED! PLAYING MUSIC...\n", index);

  if (alarmMusicTaskHandle != NULL) {
      vTaskDelete(alarmMusicTaskHandle);
      alarmMusicTaskHandle = NULL;
  }

  exitSubMenu = true;
  g_alarm_is_ringing = true;
  stopAlarmMusic = false;
  
  xTaskCreatePinnedToCore(Alarm_MusicLoop_Task, "AlarmMusicLoopTask", 8192, NULL, 1, &alarmMusicTaskHandle, 0);
}

void Alarm_StopMusic() {
    if (alarmMusicTaskHandle != NULL) {
        stopAlarmMusic = true;
        vTaskDelay(pdMS_TO_TICKS(50)); // Give task time to stop
        // No need to delete, task will delete itself.
        alarmMusicTaskHandle = NULL;
    }
    noTone(BUZZER_PIN); // Ensure sound stops immediately
    g_alarm_is_ringing = false;
    Serial.println("Alarm music stopped by user.");
    menuSprite.setTextFont(1);
    menuSprite.setTextSize(1);

}

void Alarm_Init() {
  loadAlarms();
  last_checked_day = -1;
  
  void Alarm_Check_Task(void *pvParameters); // Forward declare
  xTaskCreate(Alarm_Check_Task, "Alarm Check Task", 2048, NULL, 5, NULL);
}

void Alarm_Check_Task(void *pvParameters) {
    for(;;) {
        Alarm_Loop_Check();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
    }
}

void Alarm_Loop_Check() {
  extern struct tm timeinfo;
  if (timeinfo.tm_year < 100) return;
  
  int current_day = timeinfo.tm_wday;
  if (last_checked_day != current_day) {
    for (int i = 0; i < alarm_count; ++i) { alarms[i].triggered_today = false; }
    last_checked_day = current_day;
    saveAlarms();
  }
  
  for (int i = 0; i < alarm_count; ++i) {
    if (g_alarm_is_ringing) break; // Don't trigger a new alarm if one is already ringing
    if (alarms[i].enabled && !alarms[i].triggered_today) {
      bool day_match = alarms[i].days_of_week & (1 << current_day);
      if (day_match && alarms[i].hour == timeinfo.tm_hour && alarms[i].minute == timeinfo.tm_min) {
        triggerAlarm(i);
      }
    }
  }
}

// =====================================================================================
//                                     UI LOGIC
// =====================================================================================

void Alarm_ShowRingingScreen() {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);

    // Draw static text first
    menuSprite.setTextColor(TFT_YELLOW);
    menuSprite.setTextFont(4);
    menuSprite.drawString("Time's Up!", 120, 120); // Moved down to make space for clock

    menuSprite.setTextFont(2);
    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.drawString("Press to Stop", 120, 180); // Moved down

    unsigned long last_time_update = 0;
    bool initial_draw = true;

    while (g_alarm_is_ringing) {
        if (readButton()) {
            tone(BUZZER_PIN, 1500, 100);
            Alarm_StopMusic(); // This will set g_alarm_is_ringing to false
        }

        if (millis() - last_time_update >= 1000 || initial_draw) {
            last_time_update = millis();
            initial_draw = false;

            // Get time
            extern struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0)) { // Non-blocking get time
                 // Format time
                char time_buf[30]; // Increased buffer size
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %a", &timeinfo); // New format

                // Draw time
                menuSprite.setTextFont(4); // Or another suitable font
                menuSprite.setTextColor(TFT_WHITE, TFT_BLACK); // Draw with black background to erase old time
                menuSprite.drawString(time_buf, 120, 60); // Positioned above "Time's Up!"
                strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo); // New format
                menuSprite.setTextFont(4); // Or another suitable font
                menuSprite.setTextColor(TFT_WHITE, TFT_BLACK); // Draw with black background to erase old time
                menuSprite.drawString(time_buf, 120, 90); // Positioned above "Time's Up!"
                
            }
             menuSprite.pushSprite(0, 0); // Push the entire sprite to the screen
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void editAlarm(int index) {
    AlarmSetting temp_alarm;
    bool is_new_alarm = (index >= alarm_count);

    if (is_new_alarm) temp_alarm = {12, 0, 0, true, false};
    else temp_alarm = alarms[index];

    EditMode edit_mode = EDIT_HOUR;
    int day_cursor = 0;

    drawEditScreen(temp_alarm, edit_mode, day_cursor);

    while(true) {
        if (g_alarm_is_ringing) { return; } // Exit if alarm starts

        if (readButtonLongPress()) { 
            menuSprite.setTextFont(1); 
            menuSprite.setTextSize(2);
            return; 
        }

        int encoder_value = readEncoder();
        if (encoder_value != 0) {
            tone(BUZZER_PIN, 1000, 20);
            switch(edit_mode) {
                case EDIT_HOUR: temp_alarm.hour = (temp_alarm.hour + encoder_value + 24) % 24; break;
                case EDIT_MINUTE: temp_alarm.minute = (temp_alarm.minute + encoder_value + 60) % 60; break;
                case EDIT_DAYS: day_cursor = (day_cursor + encoder_value + 7) % 7; break;
                case EDIT_SAVE:
                case EDIT_DELETE:
                    if (encoder_value > 0) edit_mode = EDIT_DELETE;
                    if (encoder_value < 0) edit_mode = EDIT_SAVE;
                    break;
                default: break;
            }
            drawEditScreen(temp_alarm, edit_mode, day_cursor);
        }

        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50);
            if (edit_mode == EDIT_DAYS) {
                temp_alarm.days_of_week ^= (1 << day_cursor);
            } else if (edit_mode == EDIT_SAVE) {
                if (is_new_alarm) {
                    alarms[alarm_count] = temp_alarm;
                    alarm_count++;
                } else {
                    alarms[index] = temp_alarm;
                }
                saveAlarms();
                menuSprite.setTextFont(1);
                menuSprite.setTextSize(2);
                return;
            } else if (edit_mode == EDIT_DELETE) {
                if (!is_new_alarm) Alarm_Delete(index);
                menuSprite.setTextFont(1);
                menuSprite.setTextSize(2);
                return;
            }
            edit_mode = (EditMode)((edit_mode + 1) % 5);
            drawEditScreen(temp_alarm, edit_mode, day_cursor);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void AlarmMenu() {
    list_selected_index = 0;
    list_scroll_offset = 0;
    int click_count = 0;
    unsigned long last_update_time = 0;
    const unsigned long UPDATE_INTERVAL = 1000;
    menuSprite.setTextFont(1);
    menuSprite.setTextSize(2);
    drawAlarmList();

    while (true) {
        if (g_alarm_is_ringing) { return; } // Exit if alarm starts

        unsigned long current_time = millis();
        if (current_time - last_update_time >= UPDATE_INTERVAL) {
            extern struct tm timeinfo;
            if (!getLocalTime(&timeinfo)) {
                Serial.println("Failed to obtain time");
            }
            drawAlarmList();
            last_update_time = current_time;
        }

        if (readButtonLongPress()) { 
            menuSprite.setTextFont(1); 
            menuSprite.setTextSize(2);
            click_count = 0;
            return; 
        }

        int encoder_value = readEncoder();
        if (encoder_value != 0) {
            int max_items = (alarm_count < MAX_ALARMS) ? alarm_count + 1 : MAX_ALARMS;
            list_selected_index = (list_selected_index + encoder_value + max_items) % max_items;
            
            if (list_selected_index < list_scroll_offset) {
                list_scroll_offset = list_selected_index;
            } else if (list_selected_index >= list_scroll_offset + ALARMS_PER_PAGE) {
                list_scroll_offset = list_selected_index - ALARMS_PER_PAGE + 1;
            }
            
            drawAlarmList();
            tone(BUZZER_PIN, 1000 + 50 * list_selected_index, 20);
        }

        if (readButton()) { click_count++; last_click_time = millis(); }

        if (click_count > 0 && millis() - last_click_time > DOUBLE_CLICK_WINDOW) {
            if (click_count == 1) { // SINGLE CLICK
                tone(BUZZER_PIN, 2000, 50);
                if (list_selected_index < alarm_count) {
                    alarms[list_selected_index].enabled = !alarms[list_selected_index].enabled;
                    saveAlarms();
                } else { editAlarm(alarm_count); }
                drawAlarmList();
            }
            click_count = 0;
        }
        
        if (click_count >= 2) { // DOUBLE CLICK
            tone(BUZZER_PIN, 2500, 50);
            if (list_selected_index < alarm_count) {
                editAlarm(list_selected_index);
                drawAlarmList();
            }
            click_count = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// =====================================================================================
//                                     PUBLIC GETTERS
// =====================================================================================

int getAlarmCount() {
    return alarm_count;
}

bool getAlarmInfo(int index, AlarmSetting& settings) {
    if (index < 0 || index >= alarm_count) {
        return false;
    }
    settings = alarms[index];
    return true;
}
