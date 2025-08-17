#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>

// 初始化旋转编码器引脚和串口
void initRotaryEncoder();

// 读取旋转编码器状态，返回旋转增量（1: 顺时针，-1: 逆时针，0: 无变化）
int readEncoder();

// 检测按钮按下事件，返回是否按下（1: 按下，0: 未按下）
int readButton();

// 获取当前编码器计数
int getEncoderPosition();

#endif