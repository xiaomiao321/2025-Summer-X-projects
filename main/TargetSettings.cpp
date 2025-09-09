#include "TargetSettings.h"
#include "RotaryEncoder.h"
#include "Buzzer.h"
#include "weather.h" // For timeinfo
#include "Menu.h"    // For menuSprite
#include <EEPROM.h>

// --- UI Colors ---
#define TARGET_HIGHLIGHT_COLOR      TFT_YELLOW
#define TARGET_SAVE_COLOR           TFT_GREEN
#define TARGET_CANCEL_COLOR         TFT_RED

#define TARGET_PROGRESS_BAR_DATE_COLOR      TFT_YELLOW
#define TARGET_COUNTDOWN_COLOR              TFT_SKYBLUE
#define TARGET_TEXT_COLOR           TFT_GREENYELLOW
#define TARGET_PROGRESS_BAR_OUTLINE_COLOR   TFT_WHITE
#define TARGET_PROGRESS_BAR_FILL_COLOR      TFT_GREEN

// --- Module-internal State ---
static time_t countdownTarget;
static ProgressBarInfo progressBar;
static bool data_loaded = false;

// --- UI State ---
enum class EditMode { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, SAVE, CANCEL };

// --- EEPROM Data Structure ---
struct TargetData {
    uint8_t magic_key;
    time_t countdownTarget;
    ProgressBarInfo progressBar;
};

// --- Predefined Titles (Corrected "2nd") ---
const char* predefined_titles[] = {
    "1st semester",
    "winter holiday",
    "2nd semester",
    "summer holiday"
};
const int num_predefined_titles = sizeof(predefined_titles) / sizeof(predefined_titles[0]);

// =====================================================================================
//                                 FORWARD DECLARATIONS
// =====================================================================================
static void saveData();
static void loadData();
static void drawEditScreen(const tm& time, EditMode mode, const char* menuTitle);
static bool editDateTime(time_t& timeToEdit, const char* menuTitle);
static void selectTitleMenu();

// =====================================================================================
//                                     IMAGE PLACEHOLDERS
// =====================================================================================

#define ICON_WIDTH 16
#define ICON_HEIGHT 16

// Placeholder 16x16 blue square for the school icon
const uint16_t school_icon_data[ICON_WIDTH * ICON_HEIGHT] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x02e8, 0x02e8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x0000, 0x0000, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x0000, 0x0000, 0x0000, 
  0x00f8, 0x02e0, 0x02e8, 0x02e8, 0x02e8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x02e8, 0x02e8, 0x02e8, 0x02e0, 0x00f8, 
  0x00d8, 0x02e0, 0x02e8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x02e8, 0x02e0, 0x00f8, 
  0x0000, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x00f8, 0x0000, 0x0000, 0x0000, 0x0000, 0x04d8, 0x02f0, 0x02e8, 0x02e8, 0x01e0, 0x0000, 
  0x0000, 0x02e0, 0x02e0, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x0000, 0x00a8, 0x02e8, 0x02e8, 0x02e8, 0x01e0, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x02e8, 0x02e0, 0x0000, 0x02e0, 0x02f0, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x02e0, 0x03c8, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x02e0, 0x02e0, 0x00b8, 0x02e0, 0x0000, 0x0000, 0x02e0, 0x01e8, 0x0000, 0x0000, 0x02e0, 0x01e0, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x02e0, 0x02e8, 0x00f8, 0x02e0, 0x02e8, 0x0000, 0x0000, 0x0000, 0x0000, 0x02f0, 0x02e0, 0x02e8, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x02e0, 0x02f0, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x02f0, 0x02e0, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x02e8, 0x02e8, 0x02e8, 0x02e8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

// Placeholder 16x16 red square for the home icon
const uint16_t home_icon_data[ICON_WIDTH * ICON_HEIGHT] = {
 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xa0fb, 0xa0fb, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x20fb, 0xa0fb, 0xa0fb, 0x80fb, 0xa0fb, 0x00f8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x80fb, 0xa0fb, 0xa0fb, 0x0000, 0x0000, 0x80fb, 0x80fb, 0x80fb, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0xa0fb, 0xa0fb, 0x80fb, 0xa0fb, 0xa0fb, 0xa0fb, 0x80fb, 0xa0fb, 0xa0fb, 0xa0fb, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x80fb, 0xa0fb, 0xa0fb, 0xa0fb, 0x0000, 0x80fb, 0xa0fb, 0xa0fb, 0xa0fb, 0x0000, 0xc0fb, 0xa0fb, 0xa0fb, 0xa0fb, 0x0000, 
  0xa0fb, 0x80fb, 0xa0fb, 0x60fb, 0x0000, 0x0000, 0x80fb, 0x80fb, 0x80fb, 0xa0fb, 0x0000, 0x0000, 0xa0fb, 0xa0fb, 0x80fb, 0xa0fb, 
  0x80fb, 0xc0fb, 0x80fb, 0x60fb, 0x0000, 0x0000, 0x0000, 0xa0fb, 0x80fb, 0x0000, 0x0000, 0x0000, 0x80fb, 0x80fb, 0x60fb, 0xa0fb, 
  0x0000, 0x0000, 0x80fb, 0x80fb, 0x0000, 0xa0fb, 0xc0fb, 0xa0fb, 0xa0fb, 0x80fb, 0x00fc, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x80fb, 0xa0fb, 0x80fb, 0xa0fb, 0x80fb, 0x80fb, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x0000, 0x80fb, 0xa0fb, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x0000, 0x80fb, 0xa0fb, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x80fb, 0x40fb, 0x0000, 0x80fb, 0x80fb, 0x0000, 0x0000, 0x80fb, 0xe0fb, 0x0000, 0x60fb, 0x80fb, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x80fb, 0xc0fb, 0x80fb, 0xa0fb, 0x80fb, 0x80fb, 0xa0fb, 0x80fb, 0xa0fb, 0x80fb, 0xa0fb, 0x80fb, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x80fb, 0xc0fb, 0xa0fb, 0x80fb, 0xa0fb, 0xa0fb, 0xa0fb, 0x80fb, 0xa0fb, 0xa0fb, 0xc0fb, 0xa0fb, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

void drawSchoolIcon(TFT_eSprite* sprite, int x, int y) {
    sprite->pushImage(x, y, ICON_WIDTH, ICON_HEIGHT, school_icon_data);
}

void drawHomeIcon(TFT_eSprite* sprite, int x, int y) {
    sprite->pushImage(x, y, ICON_WIDTH, ICON_HEIGHT, home_icon_data);
}


// =====================================================================================
//                                     PUBLIC API
// =====================================================================================

void TargetSettings_Init() {
    if (!data_loaded) {
        loadData();
        data_loaded = true;
    }
}

time_t getCountdownTarget() {
    return countdownTarget;
}

const ProgressBarInfo& getProgressBarInfo() {
    return progressBar;
}

void drawTargetElements(TFT_eSprite* sprite) {
    if (!data_loaded) TargetSettings_Init();

    time_t now;
    time(&now);
    int y_base = sprite->height() - 80; // Start position moved down further=150
    int line_height = 10; // Consistent line height
    char buffer[64];
    struct tm time_info;

    sprite->setTextDatum(BC_DATUM);
    sprite->setTextFont(1);
    sprite->setTextSize(1);

    // --- 1. Draw Progress Bar Time Range ---
    localtime_r(&progressBar.startTime, &time_info);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &time_info);
    String start_str = buffer;
    localtime_r(&progressBar.endTime, &time_info);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &time_info);
    String end_str = buffer;
    snprintf(buffer, sizeof(buffer), "%s -> %s", start_str.c_str(), end_str.c_str());
    sprite->setTextColor(TARGET_PROGRESS_BAR_DATE_COLOR, TFT_BLACK);
    sprite->drawString(buffer, 120, y_base);

    // --- 2. Determine conditional elements ---
    bool isSemester = (strcmp(progressBar.title, "1st semester") == 0 || strcmp(progressBar.title, "2nd semester") == 0);
    const char* countdown_prefix = isSemester ? "HOME: " : "SCHOOL: ";

    // --- 3. Draw Countdown ---
    double diff_seconds = difftime(countdownTarget, now);
    if (diff_seconds > 0) {
        int days = diff_seconds / (24 * 3600);
        diff_seconds = fmod(diff_seconds, (24 * 3600));
        int hours = diff_seconds / 3600;
        diff_seconds = fmod(diff_seconds, 3600);
        int minutes = diff_seconds / 60;
        int seconds = fmod(diff_seconds, 60);

        snprintf(buffer, sizeof(buffer), "%s%d-%02d:%02d:%02d", countdown_prefix, days, hours, minutes, seconds);
        sprite->setTextColor(TARGET_COUNTDOWN_COLOR, TFT_BLACK);
        sprite->drawString(buffer, 120, y_base + line_height);
    }

    // --- 4. Draw Progress Bar ---
    if (progressBar.endTime > progressBar.startTime) {
        double total_duration = difftime(progressBar.endTime, progressBar.startTime);
        double elapsed_duration = difftime(now, progressBar.startTime);
        
        float progress = 0.0f;
        if (elapsed_duration > 0 && total_duration > 0) {
            progress = (float)elapsed_duration / (float)total_duration;
        }
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        // Draw Title & Percentage (with 4 decimal places)
        snprintf(buffer, sizeof(buffer), "%s: %.4f%%", progressBar.title, progress * 100.0f);
        sprite->setTextColor(TARGET_TEXT_COLOR, TFT_BLACK);
        sprite->drawString(buffer, 120, y_base + line_height * 2);

        // Draw Bar and Icons (Bar is wider now)
        int bar_w = 180;
        int bar_x = (sprite->width() - bar_w) / 2; // Centered
        int bar_y = y_base + line_height * 3 - 5;
        int bar_h = 10;
        int icon_size = 16;
        int icon_y = bar_y - ((icon_size - bar_h) / 2);

        sprite->drawRoundRect(bar_x, bar_y, bar_w, bar_h, 4, TARGET_PROGRESS_BAR_OUTLINE_COLOR);
        sprite->fillRoundRect(bar_x + 1, bar_y + 1, (bar_w - 2) * progress, bar_h - 2, 3, TARGET_PROGRESS_BAR_FILL_COLOR);

        // Draw conditional icons
        if (isSemester) {
            drawSchoolIcon(sprite, bar_x - icon_size - 5, icon_y);
            drawHomeIcon(sprite, bar_x + bar_w + 5, icon_y);
        } else { // Holiday
            drawHomeIcon(sprite, bar_x - icon_size - 5, icon_y);
            drawSchoolIcon(sprite, bar_x + bar_w + 5, icon_y);
        }
    }
}


// =====================================================================================
//                                 EEPROM LOGIC
// =====================================================================================

static void loadData() {
    TargetData data;
    EEPROM.get(EEPROM_TARGET_START_ADDR, data);

    if (data.magic_key == EEPROM_TARGET_MAGIC_KEY) {
        countdownTarget = data.countdownTarget;
        progressBar = data.progressBar;
    } else {
        struct tm default_tm = {0};
        default_tm.tm_year = 125; default_tm.tm_mon = 0; default_tm.tm_mday = 1;
        countdownTarget = mktime(&default_tm);
        progressBar.startTime = countdownTarget;
        progressBar.endTime = countdownTarget + (30 * 24 * 3600);
        strncpy(progressBar.title, predefined_titles[0], sizeof(progressBar.title) - 1);
        progressBar.title[sizeof(progressBar.title) - 1] = '\0';
        saveData();
    }
}

static void saveData() {
    TargetData data = {EEPROM_TARGET_MAGIC_KEY, countdownTarget, progressBar};
    EEPROM.put(EEPROM_TARGET_START_ADDR, data);
    EEPROM.commit();
}

// =====================================================================================
//                                     UI LOGIC
// =====================================================================================

void TargetSettings_Menu() {
    const char* mainMenuItems[] = {"Set Countdown", "Set Progress Start", "Set Progress End", "Set Progress Title", "Back"};
    int numItems = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);
    int selectedIndex = 0;

    while(true) {
        menuSprite.fillScreen(TFT_BLACK);
        menuSprite.setTextDatum(MC_DATUM);
        menuSprite.setTextFont(1);
        menuSprite.setTextSize(2);
        for (int i = 0; i < numItems; i++) {
            menuSprite.setTextColor(i == selectedIndex ? TARGET_HIGHLIGHT_COLOR : TARGET_TEXT_COLOR, TFT_BLACK);
            menuSprite.drawString(mainMenuItems[i], 120, 60 + i * 30);
        }
        menuSprite.pushSprite(0, 0);

        if (readButtonLongPress()) { tone(BUZZER_PIN, 1500, 100); return; }

        int encoder_value = readEncoder();
        if (encoder_value != 0) {
            selectedIndex = (selectedIndex + encoder_value + numItems) % numItems;
            tone(BUZZER_PIN, 1000, 20);
        }
        
        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50);
            bool success = false;
            switch(selectedIndex) {
                case 0: success = editDateTime(countdownTarget, "Set Countdown Target"); break;
                case 1: success = editDateTime(progressBar.startTime, "Set Progress Start"); break;
                case 2: success = editDateTime(progressBar.endTime, "Set Progress End"); break;
                case 3: selectTitleMenu(); success = true; break;
                case 4: return;
            }
            if (success) { saveData(); }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void selectTitleMenu() {
    int selectedIndex = 0;
    for(int i=0; i<num_predefined_titles; ++i) {
        if(strcmp(progressBar.title, predefined_titles[i]) == 0) { selectedIndex = i; break; }
    }

    while(true) {
        menuSprite.fillScreen(TFT_BLACK);
        menuSprite.setTextDatum(MC_DATUM);
        menuSprite.setTextFont(1);
        menuSprite.setTextSize(2);
        menuSprite.setTextColor(TARGET_TEXT_COLOR);
        menuSprite.drawString("Select Title", 120, 30);
        for (int i = 0; i < num_predefined_titles; i++) {
            menuSprite.setTextColor(i == selectedIndex ? TARGET_HIGHLIGHT_COLOR : TARGET_TEXT_COLOR, TFT_BLACK);
            menuSprite.drawString(predefined_titles[i], 120, 80 + i * 30);
        }
        menuSprite.pushSprite(0, 0);

        if (readButtonLongPress()) { tone(BUZZER_PIN, 1500, 100); return; }

        int encoder_value = readEncoder();
        if (encoder_value != 0) {
            selectedIndex = (selectedIndex + encoder_value + num_predefined_titles) % num_predefined_titles;
            tone(BUZZER_PIN, 1000, 20);
        }

        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50);
            strncpy(progressBar.title, predefined_titles[selectedIndex], sizeof(progressBar.title) - 1);
            progressBar.title[sizeof(progressBar.title) - 1] = '\0';
            saveData();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static bool editDateTime(time_t& timeToEdit, const char* menuTitle) {
    struct tm temp_tm;
    localtime_r(&timeToEdit, &temp_tm);

    EditMode mode = EditMode::YEAR;

    while(true) {
        drawEditScreen(temp_tm, mode, menuTitle);

        if (readButtonLongPress()) { tone(BUZZER_PIN, 1500, 100); return false; }

        int encoder_value = readEncoder();
        if (encoder_value != 0) {
            tone(BUZZER_PIN, 1000, 20);
            // Store original day to check for month/year roll-over
            int original_day = temp_tm.tm_mday;

            switch(mode) {
                case EditMode::YEAR:   temp_tm.tm_year += encoder_value; if(temp_tm.tm_year < 124) temp_tm.tm_year = 124; break;
                case EditMode::MONTH:  temp_tm.tm_mon  += encoder_value; break;
                case EditMode::DAY:    temp_tm.tm_mday += encoder_value; break;
                case EditMode::HOUR:   temp_tm.tm_hour = (temp_tm.tm_hour + encoder_value + 24) % 24; break;
                case EditMode::MINUTE: temp_tm.tm_min  = (temp_tm.tm_min + encoder_value + 60) % 60; break;
                case EditMode::SECOND: temp_tm.tm_sec  = (temp_tm.tm_sec + encoder_value + 60) % 60; break;
                case EditMode::SAVE:   mode = EditMode::CANCEL; break;
                case EditMode::CANCEL: mode = EditMode::SAVE; break;
            }

            // Normalize the date after changing year, month, or day
            if(mode <= EditMode::DAY) {
                // If we were editing day, and the month rolled over, clamp day to the new month's max
                if (mode == EditMode::DAY && encoder_value != 0) {
                    // Let mktime normalize month/year first
                    time_t temp_time = mktime(&temp_tm);
                    localtime_r(&temp_time, &temp_tm);
                } else {
                    // For year/month changes, just normalize
                    mktime(&temp_tm);
                }
            }
        }

        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50);
            if (mode == EditMode::SAVE) {
                timeToEdit = mktime(&temp_tm);
                return true;
            } else if (mode == EditMode::CANCEL) {
                return false;
            }
            mode = static_cast<EditMode>(static_cast<int>(mode) + 1);
            if (mode > EditMode::CANCEL) mode = EditMode::YEAR;
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // Reduced delay for more sensitivity
    }
}

static void drawEditScreen(const tm& time, EditMode mode, const char* menuTitle) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextFont(1);
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TARGET_TEXT_COLOR);
    menuSprite.drawString(menuTitle, 120, 20);

    char buf[30];
    
    snprintf(buf, sizeof(buf), "%d-%02d-%02d", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
    menuSprite.setTextFont(4); menuSprite.setTextSize(1);
    menuSprite.drawString(buf, 120, 80);
    if (mode == EditMode::YEAR) menuSprite.fillRect(30, 105, 70, 3, TARGET_HIGHLIGHT_COLOR);
    if (mode == EditMode::MONTH) menuSprite.fillRect(110, 105, 45, 3, TARGET_HIGHLIGHT_COLOR);
    if (mode == EditMode::DAY) menuSprite.fillRect(165, 105, 45, 3, TARGET_HIGHLIGHT_COLOR);

    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", time.tm_hour, time.tm_min, time.tm_sec);
    menuSprite.setTextFont(4); menuSprite.setTextSize(1);
    menuSprite.drawString(buf, 120, 140);
    if (mode == EditMode::HOUR) menuSprite.fillRect(55, 165, 45, 3, TARGET_HIGHLIGHT_COLOR);
    if (mode == EditMode::MINUTE) menuSprite.fillRect(110, 165, 45, 3, TARGET_HIGHLIGHT_COLOR);
    if (mode == EditMode::SECOND) menuSprite.fillRect(165, 165, 45, 3, TARGET_HIGHLIGHT_COLOR);

    menuSprite.setTextFont(1); menuSprite.setTextSize(2);
    menuSprite.drawRoundRect(40, 200, 75, 30, 5, TARGET_TEXT_COLOR);
    menuSprite.drawString("SAVE", 78, 215);
    if (mode == EditMode::SAVE) menuSprite.fillRoundRect(40, 200, 75, 30, 5, TARGET_SAVE_COLOR);

    menuSprite.drawRoundRect(125, 200, 75, 30, 5, TARGET_TEXT_COLOR);
    menuSprite.drawString("CANCEL", 163, 215);
    if (mode == EditMode::CANCEL) menuSprite.fillRoundRect(125, 200, 75, 30, 5, TARGET_CANCEL_COLOR);

    menuSprite.pushSprite(0, 0);
}