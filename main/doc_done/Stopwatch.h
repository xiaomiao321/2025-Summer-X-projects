#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <TFT_eSPI.h> // Assuming TFT_eSPI is needed for display
#include "RotaryEncoder.h" // Assuming RotaryEncoder is needed for input

extern TFT_eSPI tft; // Use the global tft object

void StopwatchMenu(); // Declares the main function for the stopwatch menu

#endif // STOPWATCH_H