#include "Menu.h"
#include "RotaryEncoder.h"
#include <math.h>

// 全局变量声明
extern TFT_eSPI tft;
extern int16_t display;
extern uint8_t picture_flag;
extern int16_t display_trg;
extern uint8_t circle_num;
extern const char *words[];
extern const int speed_choose;
extern const int icon_size;

// 菜单状态枚举
enum MenuState {
    MAIN_MENU,
    SUB_MENU,
    ANIMATING
};

// 全局状态变量
static MenuState current_state = MAIN_MENU;
static int16_t menu_display = 10;
static int16_t menu_display_trg = 74;
static const uint8_t ANIMATION_STEPS = 10;
static const uint8_t EASING_FACTOR = 8;

// -----------------------------
// 缓动函数（使用简单的二次缓动）
// -----------------------------
float easeOutQuad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// -----------------------------
// 动画平滑移动函数（改进版，添加缓动效果）
// -----------------------------
void ui_run_easing(int16_t *current, int16_t target, uint8_t steps) {
    if (*current == target) return;
    
    float t = (float)(ANIMATION_STEPS - steps) / ANIMATION_STEPS;
    float eased = easeOutQuad(t);
    int16_t delta = target - *current;
    *current += (int16_t)(delta * eased / steps);
    
    // 确保不超出目标值
    if (abs(*current - target) < 2) {
        *current = target;
    }
}

// -----------------------------
// 绘制主菜单图标
// -----------------------------
void drawMenuIcons(int16_t offset, int16_t y_pos) {
    tft.fillRect(0, 15, 128, 100, TFT_BLACK); // 清除图标区域
    tft.fillTriangle(64, y_pos + 26, 84, y_pos + 16, 84, y_pos + 36, TFT_WHITE);
    
    // 绘制五个彩色图标
    const uint16_t colors[] = {TFT_GREEN, TFT_BLUE, TFT_RED, TFT_YELLOW, TFT_CYAN};
    for (int i = 0; i < 5; i++) {
        tft.fillRect(offset + (i * 48), y_pos + 6, icon_size, icon_size, colors[i]);
    }
}

// -----------------------------
// 通用子菜单动画（进入/退出）
// -----------------------------
void animateMenuTransition(const char *title, bool entering) {
    current_state = ANIMATING;
    int16_t start = entering ? 10 : 74;
    int16_t target = entering ? 74 : 10;
    
    for (uint8_t step = ANIMATION_STEPS; step > 0; step--) {
        tft.fillRect(0, 5, 128, 100, TFT_BLACK); // 清除动画区域
        
        if (entering) {
            tft.drawString("MENU:", 52, menu_display);
            drawMenuIcons(display, menu_display);
        } else {
            tft.drawString(title, 52, menu_display);
            tft.drawLine(1, menu_display + 3, 128, menu_display + 3, TFT_WHITE);
        }
        
        ui_run_easing(&menu_display, target, step);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    menu_display = target;
    current_state = entering ? SUB_MENU : MAIN_MENU;
}

// -----------------------------
// 显示主菜单配置
// -----------------------------
void showMenuConfig() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    
    drawMenuIcons(display, 10);
    tft.drawString("MENU:", 52, 5);
    tft.drawString(words[picture_flag], 82, 5);
}

// -----------------------------
// 通用子菜单显示
// -----------------------------
void showSubMenu(const char *title, const char *content) {
    animateMenuTransition(title, true);
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(content, tft.width() / 2, tft.height() / 2);
    
    while (current_state == SUB_MENU) {
        if (readButton()) {
            animateMenuTransition(title, false);
            display = 48;
            picture_flag = 0;
            showMenuConfig();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// -----------------------------
// 子菜单实现
// -----------------------------
void gameMenu() {
    showSubMenu("GAME", "Game Menu");
}

void messageMenu() {
    showSubMenu("MESSAGE", "Message Menu");
}

void settingMenu() {
    showSubMenu("SETTING", "Setting Menu");
}

// -----------------------------
// 主菜单导航处理
// -----------------------------
void showMenu() {
    if (current_state != MAIN_MENU) return;
    
    int direction = readEncoder();
    
    if (direction != 0) {
        current_state = ANIMATING;
        int16_t target_display = display;
        
        if (direction == 1 && display > -144) {
            picture_flag = (picture_flag + 1) % 5;
            target_display -= 48;
        } else if (direction == -1 && display < 48) {
            picture_flag = (picture_flag == 0) ? 4 : picture_flag - 1;
            target_display += 48;
        }
        
        for (uint8_t step = ANIMATION_STEPS; step > 0; step--) {
            tft.fillRect(0, 5, 128, 100, TFT_BLACK);
            ui_run_easing(&display, target_display, step);
            drawMenuIcons(display, 10);
            tft.drawString("MENU:", 52, 5);
            tft.drawString(words[picture_flag], 82, 5);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        current_state = MAIN_MENU;
    }
    
    if (readButton()) {
        switch (picture_flag) {
            case 0: gameMenu(); break;
            case 1: weatherMenu(); break; 
            case 2: performanceMenu(); break;
            case 3: messageMenu(); break;
            case 4: settingMenu(); break;
        }
    }
}