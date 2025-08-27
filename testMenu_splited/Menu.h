#ifndef MENU_H
#define MENU_H

#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "img.h"

// Forward declarations for global variables
extern TFT_eSPI tft;
extern TFT_eSprite menuSprite;
extern int16_t display;
extern uint8_t picture_flag;

// Function prototypes for menu logic
void showMenu();
void showMenuConfig();
void animateMenuTransition(const char *title, bool entering);
void ui_run_easing(int16_t *current, int16_t target, uint8_t steps);
float easeOutQuad(float t);

// Function prototypes for individual menu screens
void CountdownMenu();
void StopwatchMenu();
void ADCMenu();
void GamesMenu();
void AnimationMenu();
void DS18B20Menu();
void performanceMenu();
void weatherMenu();
void BuzzerMenu();
void LEDMenu();
void WatchfaceMenu(); // <-- Added new watchface menu

#endif // MENU_H

