#include <Arduino.h>

// 旋转编码器引脚定义
#define ENCODER_CLK 1  // CLK 引脚
#define ENCODER_DT  0  // DT 引脚
#define ENCODER_SW  7  // SW 引脚（按钮）

// 配置选项：如果顺时针/逆时针方向相反，设置为 1
#define SWAP_CLK_DT 1  // 0: 正常，1: 交换 CLK 和 DT 逻辑

// 全局变量
volatile int encoderPos = 0;  // 旋转计数
static int buttonPressCount = 0;  // 按钮按下次数
static unsigned long lastButtonTime = 0;  // 上次按钮按下的时间
static const unsigned long debounceDelay = 100;  // 去抖动延时（毫秒）
static int lastButtonState = HIGH;  // 上次按钮状态
static uint8_t lastEncoded = 0;  // 上次编码状态
static int deltaSum = 0;  // 累积状态变化增量

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

  Serial.println("旋转编码器测试开始（GPIO 7 为 SW 引脚，CLK=GPIO 1，DT=GPIO 0）");
  Serial.println("顺时针旋转增加计数 1，逆时针旋转减少计数 1，按下按钮计数");
}

// 读取旋转编码器状态，返回旋转增量（正数为顺时针，负数为逆时针，0 为无变化）
int readEncoder() {
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

    // 完成一个周期（4 次状态变化）时更新计数
    if (deltaSum >= 4) {
      encoderPos++;
      delta = 1;
      Serial.printf("顺时针旋转，计数: %d\n", encoderPos);
      deltaSum = 0;  // 重置累积增量
    } else if (deltaSum <= -4) {
      encoderPos--;
      delta = -1;
      Serial.printf("逆时针旋转，计数: %d\n", encoderPos);
      deltaSum = 0;  // 重置累积增量
    }

    lastEncoded = currentEncoded;
  }

  return delta;
}

// 检测按钮按下事件，返回按钮按下次数（0 表示未按下）
int readButton() {
  int buttonState = digitalRead(ENCODER_SW);
  int pressCount = 0;

  if (buttonState != lastButtonState) {
    if (buttonState == LOW && millis() - lastButtonTime > debounceDelay) {
      buttonPressCount++;
      pressCount = buttonPressCount;
      Serial.printf("按钮按下，次数: %d\n", buttonPressCount);
      lastButtonTime = millis();
    }
    lastButtonState = buttonState;
  }

  return pressCount;
}

void setup() {
  initRotaryEncoder();
}

void loop() {
  // 处理旋转编码器
  readEncoder();

  // 处理按钮
  readButton();
}