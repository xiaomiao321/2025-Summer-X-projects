#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "weather.h"
#include "performance.h"
#include "LED.h"
#include "Buzzer.h"

// 全局变量声明
extern TFT_eSPI tft;
extern SemaphoreHandle_t tftMutex;
extern int16_t display;
extern uint8_t picture_flag;
extern int16_t display_trg;
extern uint8_t circle_num;
extern const char *words[];
extern const int speed_choose;
extern const int icon_size;

// 函数声明
void toSubMenuDisplay(const char *title);
void subToMenuDisplay(const char *title);
void showMenuConfig();
void gameMenu();
void messageMenu();
void settingMenu();
void showMenu();
void ui_run(int16_t *a, int16_t *a_trg, int b);
void ui_right_one_Picture(int16_t *a, int b);
void ui_left_one_Picture(int16_t *a, int b);

// Weather 和 Performance 菜单
void weatherMenu();
void performanceMenu();

#endif