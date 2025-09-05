#include "Countdown.h"
#include "Menu.h"
#include "MQTT.h"
#include "Buzzer.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include "weather.h"
// --- State Variables ---
static unsigned long countdown_target_millis = 0;
static unsigned long countdown_start_millis = 0;
static bool countdown_running = false;
static bool countdown_paused = false;
static unsigned long countdown_pause_time = 0;
static long countdown_duration_seconds = 5 * 60; // Default to 5 minutes
static unsigned long last_countdown_beep_time = 0; // For 5-second warning

// --- UI Control State ---
enum CountdownSettingMode { MODE_MINUTES, MODE_SECONDS, MODE_READY_TO_START };
static CountdownSettingMode countdown_setting_mode = MODE_MINUTES;

// --- Helper function to draw the countdown UI ---
void displayCountdownTime(unsigned long millis_left) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(TL_DATUM);

    // Display current time at the top
    // timeinfo is a global variable declared in weather.h
    if (!getLocalTime(&timeinfo)) {
        // Handle error or display placeholder
    } else {
        char time_str[30]; // Increased buffer size
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %a", &timeinfo); // New format
        menuSprite.setTextFont(2); // Smaller font for time
        menuSprite.setTextSize(1);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextDatum(MC_DATUM); // Center align
        menuSprite.drawString(time_str, menuSprite.width() / 2, 10); // Position at top center
        menuSprite.setTextDatum(TL_DATUM); // Reset datum
    }

    

    // --- Time Calculation ---
    unsigned long total_seconds = millis_left / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (millis_left % 1000) / 10;

    // --- Font and Position Calculation ---
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);
    int num_w = menuSprite.textWidth("8");
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    int num_h = menuSprite.fontHeight();
    bool show_hundredths = (countdown_running || countdown_paused);
    int total_width = (num_w * 4) + colon_w + (show_hundredths ? (dot_w + num_w * 2) : 0);
    int current_x = (menuSprite.width() - total_width) / 2;
    int y_pos = (menuSprite.height() / 2) - (num_h / 2) - 20;
    char buf[3];

    // --- Draw Minutes and Seconds with Highlighting ---
    if (!countdown_running && !countdown_paused && countdown_setting_mode == MODE_MINUTES) {
        menuSprite.setTextColor(TFT_BLACK, TFT_YELLOW);
    } else {
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    sprintf(buf, "%02ld", minutes);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK); // Colon is never highlighted
    menuSprite.drawString(":", current_x, y_pos);
    current_x += colon_w;

    if (!countdown_running && !countdown_paused && countdown_setting_mode == MODE_SECONDS) {
        menuSprite.setTextColor(TFT_BLACK, TFT_YELLOW);
    } else {
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    sprintf(buf, "%02ld", seconds);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

    // --- Draw Hundredths (if running) ---
    if (show_hundredths) {
        menuSprite.drawString(".", current_x, y_pos);
        current_x += dot_w;
        sprintf(buf, "%02ld", hundredths);
        menuSprite.drawString(buf, current_x, y_pos);
    }

    // --- Display Status Text ---
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (countdown_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (countdown_paused) {
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (millis_left == 0 && countdown_duration_seconds > 0) {
        menuSprite.drawString("FINISHED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else { // Ready to start or setting time
        if (countdown_setting_mode == MODE_MINUTES) {
            menuSprite.drawString("SET MINUTES", menuSprite.width() / 2, menuSprite.height() - 40);
        } else if (countdown_setting_mode == MODE_SECONDS) {
            menuSprite.drawString("SET SECONDS", menuSprite.width() / 2, menuSprite.height() - 40);
        } else { // MODE_READY_TO_START
            menuSprite.fillRoundRect(80, menuSprite.height() - 60, 80, 40, 5, TFT_GREEN);
            menuSprite.setTextColor(TFT_BLACK);
            menuSprite.drawString("START", menuSprite.width() / 2, menuSprite.height() - 40);
        }
    }

    // --- Draw Progress Bar ---
    float progress = (countdown_duration_seconds > 0) ? 1.0 - (float)millis_left / (countdown_duration_seconds * 1000.0) : 0.0;
    if (progress < 0) progress = 0; if (progress > 1) progress = 1;
    menuSprite.drawRect(20, menuSprite.height() / 2 + 40, menuSprite.width() - 40, 20, TFT_WHITE);
    menuSprite.fillRect(22, menuSprite.height() / 2 + 42, (int)((menuSprite.width() - 44) * progress), 16, TFT_GREEN);

    menuSprite.pushSprite(0, 0);
}

// --- Main Countdown Menu function ---
void CountdownMenu() {
    countdown_running = false;
    countdown_paused = false;
    countdown_duration_seconds = 5 * 60; // Reset to default 5 minutes
    countdown_setting_mode = MODE_MINUTES; // Start in minutes setting mode

    displayCountdownTime(countdown_duration_seconds * 1000);

    unsigned long last_display_update_time = millis();

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false;
            return;
        }
        if (g_alarm_is_ringing) { return; } 
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100);
            return;
        }

        int encoder_value = readEncoder();
        bool button_pressed = readButton();

        // --- Logic when timer is NOT running ---
        if (!countdown_running && !countdown_paused) {
            if (encoder_value != 0) {
                if (countdown_setting_mode == MODE_MINUTES) {
                    countdown_duration_seconds += encoder_value * 60;
                } else if (countdown_setting_mode == MODE_SECONDS) {
                    long current_minutes = countdown_duration_seconds / 60;
                    long current_seconds = countdown_duration_seconds % 60;
                    current_seconds += encoder_value;
                    if (current_seconds >= 60) { current_seconds = 0; current_minutes++; }
                    else if (current_seconds < 0) { current_seconds = 59; if(current_minutes > 0) current_minutes--; }
                    countdown_duration_seconds = (current_minutes * 60) + current_seconds;
                }
                if (countdown_duration_seconds < 0) countdown_duration_seconds = 0;
                displayCountdownTime(countdown_duration_seconds * 1000);
                tone(BUZZER_PIN, 1000, 20);
            }

            if (button_pressed) {
                tone(BUZZER_PIN, 2000, 50);
                if (countdown_setting_mode == MODE_READY_TO_START) {
                    // START THE TIMER
                    if (countdown_duration_seconds > 0) {
                        countdown_start_millis = millis();
                        countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                        countdown_running = true;
                        countdown_paused = false;
                        last_countdown_beep_time = 0; // Reset beep timer
                    } else { // If time is 0, just reset mode
                        countdown_setting_mode = MODE_MINUTES;
                    }
                } else {
                    // Cycle through setting modes
                    countdown_setting_mode = (CountdownSettingMode)((countdown_setting_mode + 1) % 3);
                }
                displayCountdownTime(countdown_duration_seconds * 1000);
            }
        } 
        // --- Logic when timer IS running or paused ---
        else {
            if (button_pressed) {
                tone(BUZZER_PIN, 2000, 50);
                if (countdown_running) { // PAUSE
                    countdown_pause_time = millis();
                    countdown_running = false;
                    countdown_paused = true;
                } else { // RESUME
                    countdown_start_millis += (millis() - countdown_pause_time);
                    countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                    countdown_running = true;
                    countdown_paused = false;
                }
            }
        }

        // --- Update display if running ---
        if (countdown_running) {
            unsigned long current_millis = millis();
            long millis_left = countdown_target_millis - current_millis;
            if (millis_left < 0) millis_left = 0;

            // Update countdown display frequently
            if (millis_left / 100 != (countdown_target_millis - last_display_update_time) / 100) {
                displayCountdownTime(millis_left);
                last_display_update_time = current_millis;
            }

            // 5-second warning
            if (millis_left > 0 && millis_left <= 5000 && (current_millis - last_countdown_beep_time >= 1000 || last_countdown_beep_time == 0)) {
                tone(BUZZER_PIN, 1000, 100);
                last_countdown_beep_time = current_millis;
            }

            if (millis_left == 0) {
                countdown_running = false;
                countdown_paused = false;
                tone(BUZZER_PIN, 3000, 3000); // Final alarm sound
                displayCountdownTime(0);
            }
        } else { // If countdown is not running (paused or setting time), update real-time clock
            unsigned long current_millis = millis();
            if (current_millis - last_display_update_time >= 1000) { // Update every second
                displayCountdownTime(countdown_duration_seconds * 1000); // Pass current countdown value (won't change)
                last_display_update_time = current_millis;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
