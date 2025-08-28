#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "Menu.h"

// 旋转编码器引脚定义


// 配置选项：如果顺时针/逆时针方向相反，设置为 1
#define SWAP_CLK_DT 0  // 0: 正常，1: 交换 CLK 和 DT 逻辑

// 全局变量
static volatile int encoderPos = 0;  // 旋转计数
static int buttonPressCount = 0;     // 按钮按下次数
static unsigned long lastButtonTime = 0;  // 上次按钮按下的时间
static const unsigned long debounceDelay = 50;  // 去抖动延时
static int lastButtonState = HIGH;   // 上次按钮状态
static uint8_t lastEncoded = 0;      // 上次编码状态
static int deltaSum = 0;             // 累积状态变化增量
static unsigned long lastEncoderTime = 0;  // 上次编码器读取时间
static const unsigned long encoderDebounce = 2;  // 编码器去抖动时间（毫秒）

// Global variables for long press detection
static unsigned long buttonPressStartTime = 0;
static bool longPressTriggered = false;
static const unsigned long longPressThreshold = 1000; // 1 second for long press

// 初始化旋转编码器
void initRotaryEncoder() {
  // 初始化串口
  Serial.begin(115200);

  // 设置引脚模式
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);  // GPIO 7 内部上拉

  // 读取初始状态
  lastEncoded = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);

  Serial.println("旋转编码器初始化完成（GPIO 7 为 SW 引脚，CLK=GPIO 1，DT=GPIO 0）");
}

// 读取旋转编码器状态，返回旋转增量
int readEncoder() {
  // 去抖动：检查时间间隔
  unsigned long currentTime = millis();
  if (currentTime - lastEncoderTime < 5) { // Increased debounce to 5ms
    return 0; // 间隔太短，忽略
  }
  lastEncoderTime = currentTime;

  // 读取 CLK 和 DT 状态
  int clk = digitalRead(ENCODER_CLK);
  int dt = digitalRead(ENCODER_DT);
  #if SWAP_CLK_DT
    // 交换 CLK 和 DT
    int temp = clk;
    clk = dt;
    dt = temp;
  #endif

  // 计算当前编码状态
  uint8_t currentEncoded = (clk << 1) | dt;
  int delta = 0;  // 本次旋转增量

  if (currentEncoded != lastEncoded) {
    // 状态变化表：00→10（2→0）或 11→01（3→1）等
    static const int8_t transitionTable[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    int8_t deltaValue = transitionTable[(lastEncoded << 2) | currentEncoded];
    deltaSum += deltaValue;

    // 降低阈值以提高灵敏度
    if (deltaSum >= 2) { // 原为 4
      encoderPos++;
      delta = 1;
      Serial.printf("顺时针旋转，计数: %d\n", encoderPos);
      deltaSum = 0;  // 重置累积增量
    } else if (deltaSum <= -2) { // 原为 -4
      encoderPos--;
      delta = -1;
      Serial.printf("逆时针旋转，计数: %d\n", encoderPos);
      deltaSum = 0;  // 重置累积增量
    }
  }
  lastEncoded = currentEncoded; // Moved this line outside the if block

  return delta;
}

// 检测按钮按下事件
int readButton() {
  int buttonState = digitalRead(ENCODER_SW);
  int pressed = 0;

  if (buttonState != lastButtonState) {
    if (buttonState == LOW && millis() - lastButtonTime > debounceDelay) {
      buttonPressCount++;
      pressed = 1;
      Serial.printf("按钮按下，次数: %d\n", buttonPressCount);
      lastButtonTime = millis();
    }
    lastButtonState = buttonState;
  }

  return pressed;
}

// 获取按钮的当前去抖动状态
int getButtonCurrentState() {
  return lastButtonState; // Return the debounced state maintained by readButton()
}

// New function for long press detection
bool readButtonLongPress() {
  int buttonState = digitalRead(ENCODER_SW);
  bool triggered = false;

  // Progress bar drawing variables
  static const int BAR_X = 50;
  static const int BAR_Y = 200;
  static const int BAR_WIDTH = 140;
  static const int BAR_HEIGHT = 10;
  static const int TEXT_Y = BAR_Y - 20;

  if (buttonState == LOW) { // Button is currently pressed
    if (buttonPressStartTime == 0) { // First time detecting press
      buttonPressStartTime = millis();
      longPressTriggered = false; // Reset long press flag
      // No initial clear here, let the timer display handle its own background
    }

    unsigned long currentHoldTime = millis() - buttonPressStartTime;
    float progress = (float)currentHoldTime / longPressThreshold;
    if (progress > 1.0) progress = 1.0; // Cap at 100%
    if(currentHoldTime>1000)
    {
    // Draw progress bar directly on tft (main screen)
    tft.fillRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_BLACK); // Clear bar area
    tft.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_WHITE); // Bar outline
    tft.fillRect(BAR_X, BAR_Y, (int)(BAR_WIDTH * progress), BAR_HEIGHT, TFT_BLUE); // Filled progress

    // Display "Release to Exit" text
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Release to Exit", tft.width() / 2, TEXT_Y);
    };
    // No pushSprite here, as we are drawing directly on tft

    if (!longPressTriggered && (currentHoldTime >= longPressThreshold)) {
      triggered = true;
      longPressTriggered = true; // Prevent multiple triggers for one long press
    }
  } else { // Button is not pressed
    // Clear progress bar and text when button is released
    if (buttonPressStartTime != 0) { // Only clear if it was previously pressed
      tft.fillRect(BAR_X - 5, TEXT_Y - 5, BAR_WIDTH + 10, BAR_HEIGHT + 30, TFT_BLACK); // Clear the entire area
    }
    buttonPressStartTime = 0; // Reset start time
    longPressTriggered = false; // Reset long press flag
  }

  return triggered;
}

