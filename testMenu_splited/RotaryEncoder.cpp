#include "RotaryEncoder.h"

// 旋转编码器引脚定义
#define ENCODER_CLK 1  // CLK 引脚
#define ENCODER_DT  0  // DT 引脚
#define ENCODER_SW  7  // SW 引脚（按钮）

// 配置选项：如果顺时针/逆时针方向相反，设置为 1
#define SWAP_CLK_DT 0  // 0: 正常，1: 交换 CLK 和 DT 逻辑

// 全局变量
static volatile int encoderPos = 0;  // 旋转计数
static int buttonPressCount = 0;     // 按钮按下次数
static unsigned long lastButtonTime = 0;  // 上次按钮按下的时间
static const unsigned long debounceDelay = 50;  // 去抖动延时（毫秒，降低以提高响应）
static int lastButtonState = HIGH;   // 上次按钮状态
static uint8_t lastEncoded = 0;      // 上次编码状态
static int deltaSum = 0;             // 累积状态变化增量
static unsigned long lastEncoderTime = 0;  // 上次编码器读取时间
static const unsigned long encoderDebounce = 2;  // 编码器去抖动时间（毫秒）

// 初始化旋转编码器
void initRotaryEncoder() {
  // 初始化串口
  Serial.begin(115200);
  while (!Serial);

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
  if (currentTime - lastEncoderTime < encoderDebounce) {
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

    lastEncoded = currentEncoded;
  }

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