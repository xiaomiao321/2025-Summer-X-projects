#ifndef MENU_H
#define MENU_H

#include <TFT_eSPI.h>

// 初始化 TFT 显示屏和菜单
void initMenu();

// 显示并更新菜单
void showMenu();

// 进入游戏菜单
void gameMenu();

// 菜单到游戏菜单的过渡动画
void toGameMenuDisplay();

// 游戏菜单到主菜单的过渡动画
void gameToMenuDisplay();

#endif