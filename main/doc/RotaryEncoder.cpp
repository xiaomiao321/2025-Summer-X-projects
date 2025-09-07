#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "Menu.h"

// 旋转编码器状态
static volatile int encoderPos = 0;
static uint8_t lastEncoded = 0;
static int deltaSum = 0;
static unsigned long lastEncoderTime = 0;

// 长按状态机
static unsigned long buttonPressStartTime = 0; // 按键按下的时间
static bool longPressHandled = false;          // Flag to indicate a long press has been handled for the current press

// --- Button Configuration ---
#define SWAP_CLK_DT 0
static const unsigned long longPressThreshold = 2000;     // 2 seconds to trigger a long press
static const unsigned long progressBarStartTime = 1000;   // Show progress bar after 1 second of hold

// Initializes the rotary encoder pins and state
void initRotaryEncoder() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastEncoded = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);
  Serial.println("Rotary Encoder Initialized.");
}

// Reads the encoder rotation. Returns 1 for clockwise, -1 for counter-clockwise, 0 for no change.
int readEncoder() {
  unsigned long currentTime = millis();
  if (currentTime - lastEncoderTime < 2) { // 2ms debounce
    return 0;
  }
  lastEncoderTime = currentTime;

  int clk = digitalRead(ENCODER_CLK);
  int dt = digitalRead(ENCODER_DT);
  #if SWAP_CLK_DT
    int temp = clk; clk = dt; dt = temp;
  #endif

  uint8_t currentEncoded = (clk << 1) | dt;
  int delta = 0;

  if (currentEncoded != lastEncoded) {
    static const int8_t transitionTable[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    int8_t deltaValue = transitionTable[(lastEncoded << 2) | currentEncoded];
    deltaSum += deltaValue;

    if (deltaSum >= 2) {
      encoderPos++;
      delta = 1;
      deltaSum = 0;
    } else if (deltaSum <= -2) {
      encoderPos--;
      delta = -1;
      deltaSum = 0;
    }
  }
  lastEncoded = currentEncoded;
  return delta;
}

// Detects a short button click. Returns true only on release if it wasn't a long press.
int readButton() {
    int buttonState = digitalRead(ENCODER_SW);
    bool triggered = false;

    if (buttonState == HIGH) { // Button is released
        if (buttonPressStartTime != 0) { // It was previously pressed
            // If it was a short press and not already handled as a long press
            if (!longPressHandled) {
                triggered = true;
            }
            // Reset for the next press cycle, regardless of what happened
            buttonPressStartTime = 0;
        }
    } else { // Button is pressed down
        if (buttonPressStartTime == 0) { // First moment of the press
            buttonPressStartTime = millis();
            longPressHandled = false;
        }
    }
    return triggered;
}

// Detects a long button press. Returns true once when threshold is met.
bool readButtonLongPress() {
  int buttonState = digitalRead(ENCODER_SW);
  bool triggered = false;

  static const int BAR_X = 50;
  static const int BAR_Y = 30;
  static const int BAR_WIDTH = 140;
  static const int BAR_HEIGHT = 10;

  // Only process if the button is being held and hasn't been handled as a long press yet
  if (buttonPressStartTime != 0 && !longPressHandled) {
    unsigned long currentHoldTime = millis() - buttonPressStartTime;

    // Show progress bar after the initial delay
    if (currentHoldTime >= progressBarStartTime) {
        float progress = (float)(currentHoldTime - progressBarStartTime) / (longPressThreshold - progressBarStartTime);
        if (progress > 1.0) progress = 1.0;

        menuSprite.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_WHITE);
        menuSprite.fillRect(BAR_X + 2, BAR_Y + 2, (int)((BAR_WIDTH - 4) * progress), BAR_HEIGHT - 4, TFT_BLUE);
        menuSprite.pushSprite(0,0);
    }

    // Check if the long press threshold is met
    if (currentHoldTime >= longPressThreshold) {
        triggered = true;
        longPressHandled = true; // Mark as handled
    }
  }

  // If button is released, we need to clean up the progress bar
  if (buttonState == HIGH) {
      // Check if the bar was potentially visible
      if (millis() - buttonPressStartTime < longPressThreshold && millis() - buttonPressStartTime > progressBarStartTime) {
          menuSprite.fillRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_BLACK);
          menuSprite.pushSprite(0,0);
      }
  }

  return triggered;
}