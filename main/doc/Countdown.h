#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#include <TFT_eSPI.h> // Assuming TFT_eSPI is needed for display
#include "RotaryEncoder.h" // Assuming RotaryEncoder is needed for input

extern TFT_eSPI tft; // Use the global tft object

void CountdownMenu(); // Declares the main function for the countdown menu

#endif // COUNTDOWN_H