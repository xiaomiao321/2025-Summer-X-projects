#ifndef TARGETSETTINGS_H
#define TARGETSETTINGS_H

#include <time.h>
#include <TFT_eSPI.h>

// EEPROM start address for this module's data.
// Alarm data is at address 0 and takes about 51 bytes. We'll start at 100 for safety.
#define EEPROM_TARGET_START_ADDR 100
#define EEPROM_TARGET_MAGIC_KEY 0xDA // Different magic key to avoid conflicts

// Data structure for the progress bar
struct ProgressBarInfo {
  time_t startTime;
  time_t endTime;
  char title[50]; // Title for the progress bar
};

// --- Public API ---

// Initializes the module, loading data from EEPROM.
void TargetSettings_Init();

// Displays the interactive menu for setting the countdown and progress bar.
void TargetSettings_Menu();

// Returns the current countdown target time.
time_t getCountdownTarget();

// Returns the current progress bar settings.
const ProgressBarInfo& getProgressBarInfo();

// Draws the countdown and progress bar elements onto a sprite.
void drawTargetElements(TFT_eSprite* sprite);

#endif // TARGETSETTINGS_H
