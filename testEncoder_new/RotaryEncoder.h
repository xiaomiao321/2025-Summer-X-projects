#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H
#include <TFT_eSPI.h>
#include <Arduino.h>

// 定义编码器引脚，根据实际硬件连接修改
#define ENCODER_CLK 1
#define ENCODER_DT  0
#define ENCODER_SW  7
extern TFT_eSPI tft;
extern TFT_eSprite menuSprite;
// 初始化函数
void initRotaryEncoder();

// 读取旋转方向
int readEncoder();

// 读取按键状态（已升级为状态机核心）
int readButton();

// 读取长按事件
bool readButtonLongPress();

// 读取双击事件
bool readDoubleClick();

#endif