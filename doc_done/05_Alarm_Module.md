# 闹钟模块：多功能提醒与持久化存储

## 一、引言

闹钟模块是多功能时钟的核心功能之一，它允许用户设置多个闹钟，并指定闹钟时间、重复日期。本模块旨在提供一个稳定、易于管理的闹钟系统，支持闹钟的添加、编辑、删除、启用/禁用，并将闹钟设置持久化存储，确保设备重启后闹钟信息不丢失。

## 二、实现思路与技术选型

### 2.1 闹钟数据结构与持久化

每个闹钟的设置都封装在 `AlarmSetting` 结构体中，包含时、分、重复日期、启用状态和当日是否已触发的标志。为了确保闹钟设置在设备断电后不丢失，模块利用ESP32的EEPROM（模拟Flash存储）进行持久化存储。`EEPROM.put()` 和 `EEPROM.get()` 函数用于结构体的读写。

### 2.2 多闹钟管理与日期选择

模块支持设置多个闹钟（`MAX_ALARMS`），通过一个 `AlarmSetting` 数组进行管理。闹钟的重复日期通过位掩码（Bitmask）实现，每个星期几对应一个位，方便进行逻辑“或”操作来组合重复日期，以及进行“与”操作来判断是否匹配。

### 2.3 用户界面与交互

闹钟模块提供了两个主要的UI界面：
- **闹钟列表界面**: 显示所有已设置的闹钟，包括时间、重复日期和启用状态。用户可以通过旋转编码器滚动列表，单击切换闹钟启用状态，双击进入编辑模式，或选择“Add New Alarm”添加新闹钟。
- **闹钟编辑界面**: 允许用户设置闹钟的时、分、重复日期，并提供保存和删除选项。该界面通过高亮显示当前编辑项，并利用旋转编码器进行数值调整，按键进行模式切换。

### 2.4 闹钟检查与触发

闹钟的检查逻辑在一个独立的FreeRTOS任务 `Alarm_Check_Task` 中运行，每秒检查一次当前时间是否与任何已启用的闹钟匹配。为了避免同一闹钟在同一天重复触发，引入了 `triggered_today` 标志，并在每天开始时重置。

### 2.5 闹钟响铃与停止

当闹钟触发时，会启动一个独立的FreeRTOS任务 `Alarm_MusicLoop_Task` 来播放音乐。同时，全局标志 `g_alarm_is_ringing` 被设置为 `true`，通知其他模块（如菜单、倒计时等）暂停其操作，避免干扰。用户可以通过单击按键来停止闹钟音乐，此时 `g_alarm_is_ringing` 会被重置。

## 三、代码展示

### `Alarm.cpp`

```c++
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
const uint8_t DAY_SUN = 1; const uint8_t DAY_MON = 2; const uint8_t DAY_TUE = 4;
const uint8_t DAY_WED = 8; const uint8_t DAY_THU = 16; const uint8_t DAY_FRI = 32;
const uint8_t DAY_SAT = 64; const uint8_t DAYS_ALL = 0x7F;

// --- Data Structures ---
struct AlarmSetting {
  uint8_t hour;
  uint8_t minute;
  uint8_t days_of_week;
  bool enabled;
  bool triggered_today;
};

enum EditMode { EDIT_HOUR, EDIT_MINUTE, EDIT_DAYS, EDIT_SAVE, EDIT_DELETE };

// --- Module-internal State Variables ---
static AlarmSetting alarms[MAX_ALARMS];
static int alarm_count = 0;
static int last_checked_day = -1;

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
  EEPROM.begin(sizeof(alarms) + 1);
  loadAlarms();
  last_checked_day = -1;
  
  void Alarm_Check_Task(void *pvParameters); // Forward declare
  xTaskCreate(Alarm_Check_Task, "Alarm Check Task", 2048, NULL, 5, NULL);
}

void Alarm_Check_Task(void *pvParameters) {
    for(;;)
 {
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
                } else { editAlarm(alarm_count); } // Add new alarm
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
```

## 四、代码解读

### 4.1 闹钟数据结构与全局状态

```c++
#define MAX_ALARMS 10
#define EEPROM_START_ADDR 0
#define EEPROM_MAGIC_KEY 0xAD
#define ALARMS_PER_PAGE 5

volatile bool g_alarm_is_ringing = false;

const uint8_t DAY_SUN = 1; const uint8_t DAY_MON = 2; const uint8_t DAY_TUE = 4;
const uint8_t DAY_WED = 8; const uint8_t DAY_THU = 16; const uint8_t DAY_FRI = 32;
const uint8_t DAY_SAT = 64; const uint8_t DAYS_ALL = 0x7F;

struct AlarmSetting {
  uint8_t hour;
  uint8_t minute;
  uint8_t days_of_week;
  bool enabled;
  bool triggered_today;
};

enum EditMode { EDIT_HOUR, EDIT_MINUTE, EDIT_DAYS, EDIT_SAVE, EDIT_DELETE };

static AlarmSetting alarms[MAX_ALARMS];
static int alarm_count = 0;
static int last_checked_day = -1;
```
- `MAX_ALARMS`: 定义了最大支持的闹钟数量。
- `EEPROM_START_ADDR` 和 `EEPROM_MAGIC_KEY`: 用于EEPROM存储的起始地址和魔术字，确保数据完整性。
- `ALARMS_PER_PAGE`: 定义了闹钟列表界面每页显示的闹钟数量。
- `g_alarm_is_ringing`: 全局标志，指示闹钟是否正在响铃。这是一个 `volatile` 变量，因为它会在多个任务中被访问和修改。
- `DAY_SUN` 到 `DAY_SAT`: 位掩码，用于表示星期几。`DAYS_ALL` 表示所有天。
- `AlarmSetting`: 闹钟设置结构体，包含时、分、重复日期（位掩码）、启用状态和当日是否已触发的标志。
- `EditMode`: 枚举类型，定义了闹钟编辑界面的不同编辑模式。
- `alarms`: 存储所有闹钟设置的数组。
- `alarm_count`: 当前已设置的闹钟数量。
- `last_checked_day`: 记录上次检查闹钟的日期，用于每天重置 `triggered_today` 标志。

### 4.2 EEPROM 存储与闹钟删除

```c++
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
```
- `saveAlarms()`: 将 `EEPROM_MAGIC_KEY` 和整个 `alarms` 数组写入EEPROM，并提交更改。
- `loadAlarms()`: 从EEPROM读取数据。如果魔术字匹配，则加载 `alarms` 数组并计算 `alarm_count`。否则，初始化 `alarms` 数组为默认值（所有闹钟禁用），并保存到EEPROM。
- `Alarm_Delete(int index)`: 删除指定索引的闹钟。通过将后续闹钟前移覆盖被删除的闹钟，并减少 `alarm_count` 来实现。最后将最后一个位置清空并保存。

### 4.3 闹钟音乐播放任务 `Alarm_MusicLoop_Task()`

```c++
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
```
- 这是一个FreeRTOS任务，负责循环播放预定义的音乐（来自 `Buzzer.h`）。
- `stopAlarmMusic` 标志用于控制任务的停止。当此标志为 `true` 时，任务会停止播放音乐并自行删除。
- `memcpy_P` 和 `pgm_read_word` 用于从Flash（PROGMEM）中读取歌曲数据，节省RAM。
- `tone(BUZZER_PIN, note, duration * 0.9)`: 在蜂鸣器引脚上播放指定频率和持续时间的音调。
- `vTaskDelay`: 引入延时，控制音符播放速度和歌曲间暂停。

### 4.4 闹钟检查任务 `Alarm_Check_Task()` 与 `Alarm_Loop_Check()`

```c++
void Alarm_Check_Task(void *pvParameters) {
    for(;;)
 {
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
```
- `Alarm_Check_Task`: 一个FreeRTOS任务，每秒调用 `Alarm_Loop_Check()` 来检查闹钟。
- `Alarm_Loop_Check()`:
    - **日期重置**: 如果当前日期与 `last_checked_day` 不同，说明进入了新的一天，此时会遍历所有闹钟，将 `triggered_today` 标志重置为 `false`，并保存闹钟设置。
    - **闹钟匹配**: 遍历所有已设置的闹钟。如果闹钟已启用 (`enabled`) 且当天未触发 (`!triggered_today`)，并且当前时间（小时和分钟）与闹钟设置匹配，同时当前星期几与闹钟的重复日期匹配（通过位运算 `&`），则调用 `triggerAlarm()` 触发闹钟。
    - `g_alarm_is_ringing` 检查: 如果已有闹钟正在响铃，则不再触发新的闹钟。

### 4.5 闹钟触发与停止

```c++
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
```
- `triggerAlarm(int index)`:
    - 将触发的闹钟的 `triggered_today` 标志设置为 `true` 并保存，防止当天重复触发。
    - 如果之前有闹钟音乐任务在运行，先删除它。
    - 设置 `exitSubMenu` 为 `true`，强制退出当前子菜单。
    - 设置 `g_alarm_is_ringing` 为 `true`，通知其他模块闹钟正在响铃。
    - 创建 `Alarm_MusicLoop_Task` 任务来播放闹钟音乐。
- `Alarm_StopMusic()`:
    - 设置 `stopAlarmMusic` 标志为 `true`，通知音乐任务停止。
    - 短暂延时，等待音乐任务自行删除。
    - `noTone(BUZZER_PIN)`: 立即停止蜂鸣器发声。
    - 将 `g_alarm_is_ringing` 设置为 `false`，表示闹钟已停止。

### 4.6 闹钟响铃界面 `Alarm_ShowRingingScreen()`

```c++
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
```
- 当 `g_alarm_is_ringing` 为 `true` 时，此函数被调用，显示一个全屏的闹钟响铃界面。
- 界面显示“Time's Up!”和“Press to Stop”提示。
- 实时显示当前时间，每秒更新一次。
- 用户单击按键时，调用 `Alarm_StopMusic()` 停止闹钟。

### 4.7 闹钟编辑界面 `editAlarm()`

```c++
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
```
- `editAlarm(int index)`: 用于编辑现有闹钟或添加新闹钟。
- `is_new_alarm`: 判断是添加新闹钟还是编辑旧闹钟。
- `EditMode`: 通过 `edit_mode` 变量控制当前编辑的是小时、分钟、星期、保存还是删除。
- **旋转编码器**: 用于调整当前编辑项的数值（小时、分钟、星期光标）。
- **按键**: 
    - 在小时、分钟、星期模式下，单击按键切换到下一个编辑模式。
    - 在星期模式下，单击按键切换当前星期日的选中状态（通过位异或 `^=`）。
    - 在保存模式下，单击按键保存闹钟并退出。
    - 在删除模式下，单击按键删除闹钟并退出。
- `drawEditScreen()`: 负责绘制编辑界面，并根据 `edit_mode` 和 `day_cursor` 高亮显示当前编辑项。

### 4.8 闹钟主菜单 `AlarmMenu()`

```c++
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
                } else { editAlarm(alarm_count); } // Add new alarm
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
```
- `AlarmMenu()`: 闹钟模块的主入口函数，负责显示闹钟列表并处理用户交互。
- **初始化**: 重置选择索引、滚动偏移和单击计数。
- **实时时间更新**: 每秒更新一次屏幕顶部的实时时间。
- **旋转编码器**: 用于在闹钟列表中上下选择闹钟或“Add New Alarm”选项，并处理列表滚动。
- **按键**: 
    - **长按**: 退出闹钟菜单。
    - **单击/双击检测**: 通过 `click_count` 和 `DOUBLE_CLICK_WINDOW` 实现单击和双击的区分。
        - **单击**: 切换当前选中闹钟的启用/禁用状态，或在选中“Add New Alarm”时进入添加模式。
        - **双击**: 进入当前选中闹钟的编辑模式。
- `drawAlarmList()`: 负责绘制闹钟列表界面，并高亮显示当前选中项。

## 五、总结与展望

闹钟模块提供了一个功能全面且用户友好的闹钟管理系统。通过EEPROM持久化存储、FreeRTOS任务进行后台检查、以及清晰的UI交互，确保了闹钟功能的稳定性和可靠性。

未来的改进方向：
1.  **贪睡功能**: 增加闹钟响铃后的贪睡（Snooze）选项。
2.  **自定义闹钟音效**: 允许用户选择不同的内置音乐或上传自定义音乐作为闹钟铃声。
3.  **闹钟名称**: 允许用户为每个闹钟设置一个简短的名称，方便识别。
4.  **振动提醒**: 如果设备支持振动马达，可以增加振动提醒功能。
5.  **更丰富的UI**: 增加闹钟列表的动画效果，或在响铃界面显示闹钟名称。

谢谢大家
