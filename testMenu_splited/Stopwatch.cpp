#include "Stopwatch.h"
#include "Menu.h"
#include "MQTT.h"
#include "Buzzer.h"
#include "Alarm.h"
#include "RotaryEncoder.h"

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
    displayStopwatchTime(0);

    bool button_pressed = false;
    unsigned long last_display_update_time = millis();


    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            return; // Exit the StopwatchMenu function
        }
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        button_pressed = readButton();

        // Button press handling
        if (button_pressed) {
            tone(BUZZER_PIN, 2000, 50); // Confirm sound
            if (!stopwatch_running && stopwatch_elapsed_time == 0) { // Start
                stopwatch_start_time = millis();
                stopwatch_running = true;
            } else if (stopwatch_running) { // Stop/Pause
                stopwatch_elapsed_time += (millis() - stopwatch_start_time);
                stopwatch_running = false;
            } else if (!stopwatch_running && stopwatch_elapsed_time > 0) { // Reset or Resume
                // If already stopped, a press resets it
                // If paused, a press resumes it
                if (stopwatch_elapsed_time > 0 && (millis() - stopwatch_pause_time) > 500) { // Simple debounce for reset
                    // If button pressed again after a short pause, it's a reset
                    stopwatch_start_time = 0;
                    stopwatch_elapsed_time = 0;
                    stopwatch_running = false;
                    displayStopwatchTime(0);
                } else { // Resume
                    stopwatch_start_time = millis() - stopwatch_elapsed_time; // Adjust start time
                    stopwatch_running = true;
                }
            }
            stopwatch_pause_time = millis(); // Record time of last button press
            displayStopwatchTime(stopwatch_elapsed_time); // Update display immediately
        }

        // Update stopwatch if running
        if (stopwatch_running) {
            unsigned long current_elapsed = millis() - stopwatch_start_time;
            if (current_elapsed / 10 != stopwatch_elapsed_time / 10) { // Update every hundredth of a second
                displayStopwatchTime(stopwatch_elapsed_time + current_elapsed);
            }
        }

        // Exit condition using long press
        if (readButtonLongPress()) {
            menuSprite.setTextFont(1);
            tone(BUZZER_PIN, 1500, 100); // Exit sound
            return; // Exit the StopwatchMenu function
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
}