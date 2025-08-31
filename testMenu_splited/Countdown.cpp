#include "Countdown.h"
#include "Menu.h"
#include "MQTT.h"
#include "Buzzer.h"
#include "RotaryEncoder.h"

#define LONG_PRESS_DURATION 1500 // milliseconds for long press to exit

// Global variables for countdown state
static unsigned long countdown_target_millis = 0;
static unsigned long countdown_start_millis = 0;
static bool countdown_running = false;
static bool countdown_paused = false;
static unsigned long countdown_pause_time = 0;
static long countdown_duration_seconds = 5 * 60; // Default to 5 minutes

// Global variables for UI control
static int countdown_setting_mode = 0; // 0: minutes, 1: seconds
static unsigned long lastClickTime = 0;
static int clickCount = 0;
static const unsigned long doubleClickTimeWindow = 300; // ms

// Function to display the countdown time with dynamic layout
// Added current_hold_duration for long press progress bar
void displayCountdownTime(unsigned long millis_left) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(TL_DATUM); // Use Top-Left for precise positioning

    // Time calculation
    unsigned long total_seconds = millis_left / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (millis_left % 1000) / 10;

    // Font settings for time display
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);

    // Character width calculation for dynamic positioning
    int num_w = menuSprite.textWidth("8"); // Use a wide character for consistent spacing
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    int num_h = menuSprite.fontHeight();

    // Determine total width based on whether to show hundredths
    int total_width;
    bool show_hundredths = (countdown_running || countdown_paused);
    if (show_hundredths) {
        total_width = (num_w * 4) + colon_w + dot_w + (num_w * 2);
    } else {
        total_width = (num_w * 4) + colon_w;
    }

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

    // Draw Hundredths if the timer is active
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
    } else {
        menuSprite.drawString("READY", menuSprite.width() / 2, menuSprite.height() - 80);
        if (countdown_setting_mode == 0) {
            menuSprite.drawString("Set Mins", menuSprite.width() / 2, menuSprite.height() - 40);
        } else {
            menuSprite.drawString("Set Secs", menuSprite.width() / 2, menuSprite.height() - 40);
        }
    }

    // --- Draw Progress Bar ---
    int bar_x = 20;
    int bar_y = menuSprite.height() / 2 + 40;
    int bar_width = menuSprite.width() - 40;
    int bar_height = 20;

    float progress = 0.0;
    if (countdown_duration_seconds > 0) {
        progress = 1.0 - (float)millis_left / (countdown_duration_seconds * 1000.0);
    }
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;

    menuSprite.drawRect(bar_x, bar_y, bar_width, bar_height, TFT_WHITE);
    menuSprite.fillRect(bar_x + 2, bar_y + 2, (int)((bar_width - 4) * progress), bar_height - 4, TFT_GREEN);

    menuSprite.pushSprite(0, 0);
}

// Main Countdown Menu function
void CountdownMenu() {
    countdown_running = false;
    countdown_paused = false;
    countdown_duration_seconds = 5 * 60; // Reset to default 5 minutes
    countdown_setting_mode = 0; // Start in minutes setting mode

    displayCountdownTime(countdown_duration_seconds * 1000);

    int encoder_value = 0;
    bool button_pressed = false;
    unsigned long last_display_update_time = millis();


    while (true) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            return; // Exit the CountdownMenu function
        }
        encoder_value = readEncoder();
        button_pressed = readButton();

        // Exit condition using long press
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100); // Exit sound
            menuSprite.fillScreen(TFT_BLACK); // Clear the screen
            menuSprite.pushSprite(0, 0);
            menuSprite.setTextFont(1);
            return; // Exit the CountdownMenu function
        }

        // Adjust countdown duration if not running or paused
        if (!countdown_running && !countdown_paused) {
            if (encoder_value != 0) {
                if (countdown_setting_mode == 0) { // Adjust minutes
                    countdown_duration_seconds += encoder_value * 60;
                } else { // Adjust seconds independently
                    long current_minutes = countdown_duration_seconds / 60;
                    long current_seconds = countdown_duration_seconds % 60;

                    current_seconds += encoder_value;

                    // Handle seconds rollover
                    if (current_seconds >= 60) {
                        current_seconds = 0;
                        current_minutes++;
                    } else if (current_seconds < 0) {
                        current_seconds = 59;
                        current_minutes--;
                        if (current_minutes < 0) current_minutes = 0; // Don't go negative minutes
                    }
                    countdown_duration_seconds = (current_minutes * 60) + current_seconds;
                }
                if (countdown_duration_seconds < 0) countdown_duration_seconds = 0;
                displayCountdownTime(countdown_duration_seconds * 1000);
                tone(BUZZER_PIN, 1000, 20); // Feedback
            }
        }

        // Button press handling (short press and double-click)
        if (button_pressed) {
            tone(BUZZER_PIN, 2000, 50); // Confirm sound

            unsigned long currentTime = millis();
            if (currentTime - lastClickTime < doubleClickTimeWindow) {
                clickCount++;
            } else {
                clickCount = 1; // First click in a new sequence
            }
            lastClickTime = currentTime;

            if (clickCount == 2) { // Double-click detected
                clickCount = 0; // Reset for next double-click
                if (!countdown_running && !countdown_paused) { // Only toggle setting mode if not running/paused
                    countdown_setting_mode = 1 - countdown_setting_mode; // Toggle between 0 (minutes) and 1 (seconds)
                    displayCountdownTime(countdown_duration_seconds * 1000); // Update display to show mode
                }
            } else { // Single click (or first click of a potential double-click)
                if (!countdown_running && !countdown_paused) { // Timer is READY or FINISHED
                    if (countdown_duration_seconds == 0) { // If finished, reset
                        countdown_duration_seconds = 5 * 60; // Reset to default
                        displayCountdownTime(countdown_duration_seconds * 1000);
                    } else { // Timer is READY and duration > 0, so start countdown
                        countdown_start_millis = millis();
                        countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                        countdown_running = true;
                        countdown_paused = false;
                    }
                } else if (countdown_running) { // Pause countdown
                    countdown_pause_time = millis();
                    countdown_running = false;
                    countdown_paused = true;
                } else if (countdown_paused) { // Resume countdown
                    countdown_start_millis += (millis() - countdown_pause_time); // Adjust start time
                    countdown_target_millis = countdown_start_millis + (countdown_duration_seconds * 1000);
                    countdown_running = true;
                    countdown_paused = false;
                }
                unsigned long current_millis = millis();
                long millis_left = countdown_target_millis - current_millis;
                if(millis_left < 0) millis_left = 0;
                displayCountdownTime(millis_left); // Update display immediately
            }
        }

        // Reset clickCount if time window expires without a second click
        if (millis() - lastClickTime > doubleClickTimeWindow && clickCount == 1) {
            clickCount = 0; // Reset after single click
        }

        // Update countdown if running
        if (countdown_running) {
            unsigned long current_millis = millis();
            long millis_left = countdown_target_millis - current_millis;

            if (millis_left < 0) millis_left = 0;

            // Update every hundredth of a second
            if (millis_left / 10 != (countdown_target_millis - last_display_update_time) / 10) {
                displayCountdownTime(millis_left);
                last_display_update_time = current_millis;
            }

            if (millis_left == 0) {
                countdown_running = false;
                countdown_paused = false;
                tone(BUZZER_PIN, 500, 1000); // Alarm sound
                displayCountdownTime(0); // Display 00:00.00
                // No automatic exit, stays on 00:00 until button press to reset
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
}
