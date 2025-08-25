#include "Countdown.h"
#include "Menu.h" // To return to the main menu (though not directly used for exit here)
#include "Buzzer.h" // For sound feedback
#include "RotaryEncoder.h" // For readButtonLongPress

// Global variables for countdown
static unsigned long countdown_target_millis = 0;
static unsigned long countdown_start_millis = 0;
static bool countdown_running = false;
static bool countdown_paused = false;
static unsigned long countdown_pause_time = 0;

// Initial countdown duration in seconds (e.g., 5 minutes for testing)
static long countdown_duration_seconds = 5 * 60; // Default to 5 minutes

// Global variable to track setting mode (0: minutes, 1: seconds)
static int countdown_setting_mode = 0;

static unsigned long lastClickTime = 0;
static int clickCount = 0;
static const unsigned long doubleClickTimeWindow = 300; // milliseconds

// Function to display the countdown time
void displayCountdownTime(long seconds_left) {
    menuSprite.fillScreen(TFT_BLACK); // Use menuSprite
    menuSprite.setTextFont(4); // Large font
    menuSprite.setTextDatum(MC_DATUM); // Middle center alignment

    char buf[20];
    long minutes = seconds_left / 60;
    long seconds = seconds_left % 60;

    sprintf(buf, "%02ld:%02ld", minutes, seconds); // Only minutes and seconds
    menuSprite.drawString(buf, menuSprite.width() / 2, menuSprite.height() / 2 - 20);

    // Display status
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (countdown_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 20);
    } else if (countdown_paused) {
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 20);
    } else if (seconds_left == 0 && !countdown_running && !countdown_paused) {
        menuSprite.drawString("FINISHED", menuSprite.width() / 2, menuSprite.height() - 20);
    }
    else {
        menuSprite.drawString("READY", menuSprite.width() / 2, menuSprite.height() - 20);
        // Indicate current setting mode
        if (countdown_setting_mode == 0) {
            menuSprite.drawString("MIN", menuSprite.width() / 2, menuSprite.height() - 40);
        } else {
            menuSprite.drawString("SEC", menuSprite.width() / 2, menuSprite.height() - 40);
        }
    }

    // Draw Progress Bar
    int bar_x = 20;
    int bar_y = menuSprite.height() / 2 + 40;
    int bar_width = menuSprite.width() - 40;
    int bar_height = 10;

    float progress = 0.0;
    if (countdown_duration_seconds > 0) {
        progress = 1.0 - (float)seconds_left / countdown_duration_seconds; // Calculate progress (0 to 1)
    }

    menuSprite.drawRect(bar_x, bar_y, bar_width, bar_height, TFT_WHITE); // Outline
    menuSprite.fillRect(bar_x, bar_y, (int)(bar_width * progress), bar_height, TFT_GREEN); // Filled part

    menuSprite.pushSprite(0, 0); // Push sprite to screen
}

// Main Countdown Menu function
void CountdownMenu() {
    // Reset state when entering the menu
    countdown_running = false;
    countdown_paused = false;
    countdown_duration_seconds = 5 * 60; // Reset to default 5 minutes
    countdown_setting_mode = 0; // Start in minutes setting mode
    displayCountdownTime(countdown_duration_seconds);

    int encoder_value = 0;
    bool button_pressed = false;
    unsigned long last_display_update_time = millis();

    while (true) {
        encoder_value = readEncoder();
        button_pressed = readButton();

        // Exit condition using long press
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100); // Exit sound
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
                displayCountdownTime(countdown_duration_seconds);
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
                    displayCountdownTime(countdown_duration_seconds); // Update display to show mode
                }
            } else { // Single click (or first click of a potential double-click)
                if (!countdown_running && !countdown_paused) { // Timer is READY or FINISHED
                    if (countdown_duration_seconds == 0) { // If finished, reset
                        countdown_duration_seconds = 5 * 60; // Reset to default
                        displayCountdownTime(countdown_duration_seconds);
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
                displayCountdownTime((countdown_target_millis - millis()) / 1000); // Update display immediately
            }
        }

        // Reset clickCount if time window expires without a second click
        if (millis() - lastClickTime > doubleClickTimeWindow && clickCount == 1) {
            clickCount = 0; // Reset after single click
        }

        // Update countdown if running
        if (countdown_running) {
            unsigned long current_millis = millis();
            long seconds_left = (countdown_target_millis - current_millis) / 1000;

            if (seconds_left < 0) seconds_left = 0; // Ensure it doesn't go negative

            if (seconds_left != (last_display_update_time - countdown_start_millis) / 1000) { // Only update if second changes
                displayCountdownTime(seconds_left);
                last_display_update_time = current_millis;
            }

            if (seconds_left == 0) {
                countdown_running = false;
                countdown_paused = false;
                tone(BUZZER_PIN, 500, 1000); // Alarm sound
                displayCountdownTime(0); // Display 00:00
                // No automatic exit, stays on 00:00 until button press to reset
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
}
