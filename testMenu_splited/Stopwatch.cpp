#include "Stopwatch.h"
#include "Menu.h"
#include "MQTT.h"
#include "Buzzer.h"
#include "Alarm.h"
#include "RotaryEncoder.h"
#include "weather.h"
#define LONG_PRESS_DURATION 1500 // milliseconds for long press to exit

// Global variables for stopwatch state
static unsigned long stopwatch_start_time = 0;
static unsigned long stopwatch_elapsed_time = 0;
static bool stopwatch_running = false;
static unsigned long stopwatch_pause_time = 0;

// Function to display the stopwatch time with dynamic layout
// Added current_hold_duration for long press progress bar
void displayStopwatchTime(unsigned long elapsed_millis) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(TL_DATUM); // Use Top-Left for precise positioning

    // Display current time at the top
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

    // Time calculation
    unsigned long total_seconds = elapsed_millis / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (elapsed_millis % 1000) / 10;

    // Font settings for time display
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);

    // Character width calculation for dynamic positioning
    int num_w = menuSprite.textWidth("8"); // Use a wide character for consistent spacing
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    int num_h = menuSprite.fontHeight();

    // Calculate total width for "MM:SS.hh"
    int total_width = (num_w * 4) + colon_w + dot_w + (num_w * 2);

    // Calculate centered starting position
    int current_x = (menuSprite.width() - total_width) / 2;
    int y_pos = (menuSprite.height() / 2) - (num_h / 2) - 20;

    char buf[3];

    // Draw Minutes
    sprintf(buf, "%02ld", minutes);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    // Draw Colon
    menuSprite.drawString(":", current_x, y_pos);
    current_x += colon_w;

    // Draw Seconds
    sprintf(buf, "%02ld", seconds);
    menuSprite.drawString(buf, current_x, y_pos);
    current_x += num_w * 2;

    // Draw Dot
    menuSprite.drawString(".", current_x, y_pos);
    current_x += dot_w;

    // Draw Hundredths
    sprintf(buf, "%02ld", hundredths);
    menuSprite.drawString(buf, current_x, y_pos);

    // --- Display Status Text ---
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (stopwatch_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 80);
    } else if (elapsed_millis > 0) {
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 80);
    } else {
        menuSprite.drawString("READY", menuSprite.width() / 2, menuSprite.height() - 80);
    }


    menuSprite.pushSprite(0, 0);
}

// Main Stopwatch Menu function
void StopwatchMenu() {
    // Reset state when entering the menu
    stopwatch_start_time = 0;
    stopwatch_elapsed_time = 0;
    stopwatch_running = false;
    stopwatch_pause_time = 0;
    displayStopwatchTime(0); // Initial display
    static unsigned long last_displayed_stopwatch_millis = 0; // Track last displayed value for smooth updates
    unsigned long last_realtime_clock_update = millis(); // For real-time clock update

    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            return; // Exit the StopwatchMenu function
        }
        if (g_alarm_is_ringing) { return; }

        // Handle long press for reset and exit
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100); // Exit sound
            // Reset stopwatch state
            stopwatch_start_time = 0;
            stopwatch_elapsed_time = 0;
            stopwatch_running = false;
            stopwatch_pause_time = 0;
            last_displayed_stopwatch_millis = 0; // Reset this as well
            return; // Exit the StopwatchMenu function
        }

        // Handle single click for start/pause
        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50); // Confirm sound
            if (stopwatch_running) { // Currently running, so pause
                stopwatch_elapsed_time += (millis() - stopwatch_start_time);
                stopwatch_running = false;
            } else { // Currently paused or stopped, so start/resume
                stopwatch_start_time = millis(); // Adjust start time for resume
                stopwatch_running = true;
            }
            // IMMEDIATE DISPLAY UPDATE AFTER STATE CHANGE
            unsigned long current_display_value;
            if (stopwatch_running) {
                current_display_value = stopwatch_elapsed_time + (millis() - stopwatch_start_time);
            } else {
                current_display_value = stopwatch_elapsed_time;
            }
            displayStopwatchTime(current_display_value);
            last_displayed_stopwatch_millis = current_display_value; // Update this immediately
        }

        // Update stopwatch display (high frequency)
        unsigned long current_stopwatch_display_millis;
        if (stopwatch_running) {
            current_stopwatch_display_millis = stopwatch_elapsed_time + (millis() - stopwatch_start_time);
        } else {
            current_stopwatch_display_millis = stopwatch_elapsed_time;
        }
        
        // Update stopwatch display if hundredths changed OR if real-time clock needs update
        // This ensures both stopwatch and real-time clock are updated.
        if (current_stopwatch_display_millis / 10 != last_displayed_stopwatch_millis / 10 || (millis() - last_realtime_clock_update) >= 1000) {
            displayStopwatchTime(current_stopwatch_display_millis);
            last_displayed_stopwatch_millis = current_stopwatch_display_millis; // Update last displayed value
            if ((millis() - last_realtime_clock_update) >= 1000) {
                last_realtime_clock_update = millis(); // Update real-time clock update time
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
}