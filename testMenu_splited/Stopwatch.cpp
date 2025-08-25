#include "Stopwatch.h"
#include "Menu.h" // To return to the main menu (though not directly used for exit here)
#include "Buzzer.h" // For sound feedback
#include "RotaryEncoder.h" // For readButtonLongPress

// Global variables for stopwatch
static unsigned long stopwatch_start_time = 0;
static unsigned long stopwatch_elapsed_time = 0;
static bool stopwatch_running = false;
static unsigned long stopwatch_pause_time = 0;

// Function to display the stopwatch time
void displayStopwatchTime(unsigned long elapsed_millis) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextFont(4); // Large font
    menuSprite.setTextDatum(MC_DATUM); // Middle center alignment

    char buf[20];
    unsigned long total_seconds = elapsed_millis / 1000;
    long minutes = total_seconds / 60;
    long seconds = total_seconds % 60;
    long hundredths = (elapsed_millis % 1000) / 10; // Display hundredths

    sprintf(buf, "%02ld:%02ld.%02ld", minutes, seconds, hundredths);
    menuSprite.drawString(buf, menuSprite.width() / 2, menuSprite.height() / 2 - 20);

    // Display status
    menuSprite.setTextFont(2);
    menuSprite.setTextDatum(BC_DATUM);
    if (stopwatch_running) {
        menuSprite.drawString("RUNNING", menuSprite.width() / 2, menuSprite.height() - 20);
    } else if (stopwatch_elapsed_time > 0 && !stopwatch_running) { // Paused or stopped after running
        menuSprite.drawString("PAUSED", menuSprite.width() / 2, menuSprite.height() - 20);
    }
    else {
        menuSprite.drawString("READY", menuSprite.width() / 2, menuSprite.height() - 20);
    }

    menuSprite.pushSprite(0, 0); // Push sprite to screen
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
            tone(BUZZER_PIN, 1500, 100); // Exit sound
            return; // Exit the StopwatchMenu function
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }
}