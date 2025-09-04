#ifndef ALARM_H
#define ALARM_H

// 初始化闹钟模块，在 setup() 中调用
void Alarm_Setup();

// 在主 loop() 中持续调用，用于在后台检查闹钟时间是否到达
void Alarm_Loop_Check();

// 闹钟设置界面的主函数
void AlarmMenu();

// 停止当前正在播放的闹钟音乐
void Alarm_StopMusic();

#endif // ALARM_H
