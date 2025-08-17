#ifndef MENU_H
#define MENU_H

#include <TFT_eSPI.h>

// 初始化 TFT 显示屏和菜单
void initMenu();

// 根据旋转方向更新菜单选择或设置值
void updateMenu(int direction);

// 处理按钮按下事件，切换菜单或返回主菜单
void handleButtonPress();

#endif