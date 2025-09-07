#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "Menu.h"

// --- Rotary Encoder State ---
static volatile int encoderPos = 0;
static uint8_t lastEncoded = 0;
static int deltaSum = 0;
static unsigned long lastEncoderTime = 0;

// --- Button State Machine Variables ---
static unsigned long buttonPressStartTime = 0; // When the button was pressed down
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
// int readEncoder() {
//   unsigned long currentTime = millis();
//   if (currentTime - lastEncoderTime < 2) { // 2ms debounce
//     return 0;
//   }
//   lastEncoderTime = currentTime;

//   int clk = digitalRead(ENCODER_CLK);
//   int dt = digitalRead(ENCODER_DT);
//   #if SWAP_CLK_DT
//     int temp = clk; clk = dt; dt = temp;
//   #endif

//   uint8_t currentEncoded = (clk << 1) | dt;
//   int delta = 0;

//   if (currentEncoded != lastEncoded) {
//     static const int8_t transitionTable[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
//     int8_t deltaValue = transitionTable[(lastEncoded << 2) | currentEncoded];
//     deltaSum += deltaValue;

//     if (deltaSum >= 2) {
//       encoderPos++;
//       delta = 1;
//       deltaSum = 0;
//     } else if (deltaSum <= -2) {
//       encoderPos--;
//       delta = -1;
//       deltaSum = 0;
//     }
//   }
//   lastEncoded = currentEncoded;
//   return delta;
// }
int readEncoder() {
   static int lastCLK_State = HIGH; // 静态变量，保存CLK引脚上一次的状态
   int currentCLK_State = digitalRead(ENCODER_CLK); // 读取CLK引脚当前状态
   int delta = 0; // 返回值，0=无变化，1=顺时针，-1=逆时针
   // 检查CLK引脚的状态是否发生了变化（即出现了边沿）
   if (currentCLK_State != lastCLK_State) {
     // 简单软件消抖：如果状态变化了，等待一小段时间再确认，避免抖动
     delayMicroseconds(500); // 等待500微秒，这是一个常用的消抖时间
     // 再次读取CLK和DT引脚，确保状态稳定
     currentCLK_State = digitalRead(ENCODER_CLK);
     int currentDT_State = digitalRead(ENCODER_DT);
     #if SWAP_CLK_DT
       // 如果配置了交换引脚，则交换CLK和DT的角色
       int temp = currentCLK_State;
       currentCLK_State = currentDT_State;
       currentDT_State = temp;
     #endif
     // 判断逻辑：只有在CLK为 LOW 时判断DT的状态，方向才最准确
     // 你也可以在CLK为 HIGH 时判断，但需要与下面的判断条件相反
     if (currentCLK_State == LOW) {
       if (currentDT_State == LOW) {
         // CLK为低时DT也为低 -> 顺时针旋转
         delta = 1;
         encoderPos++;
       } else {
         // CLK为低时DT为高 -> 逆时针旋转
         delta = -1;
         encoderPos--;
       }
     }
     // 更新上一次的CLK状态，为下一次判断做准备
     lastCLK_State = currentCLK_State;
   }
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