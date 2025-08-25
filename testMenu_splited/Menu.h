#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "img.h"
extern TFT_eSPI tft;
extern TFT_eSprite menuSprite; 
extern int16_t display;              // 图标初始x偏移
extern uint8_t picture_flag;          // 当前选中的菜单项索引
void showMenu();
void showMenuConfig();
void animateMenuTransition(const char *title, bool entering);
void ui_run_easing(int16_t *current, int16_t target, uint8_t steps);
float easeOutQuad(float t);

// New function declarations for the menu items
void CountdownMenu();
void StopwatchMenu();
void ADCMenu();
