#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
#include "img.h"
#include "LED.h"
#include "Buzzer.h"
#include "Alarm.h"
#include "Pomodoro.h"
#include "weather.h"
#include "performance.h"
#include "DS18B20.h"
#include "animation.h"
#include "Games.h"
#include "ADC.h"
#include "Watchface.h" 
#include "MQTT.h"
#include "MusicMenuLite.h"/* 引入所有模块的实现文件 */
// 布局设置变量
static const int ICON_SIZE = 200;     // 图标大小(假设为正方形)
static const int ICON_SPACING = 220;  // 图表间的水平距离(应该大于ICON_SIZE)
static const int SCREEN_WIDTH = 240;  // 屏幕宽度
static const int SCREEN_HEIGHT = 240; // 屏幕高度

// 计算布局变量
static const int ICON_Y_POS = (SCREEN_HEIGHT / 2) - (ICON_SIZE / 2); // 图标Y放在最中间

static const int INITIAL_X_OFFSET = (SCREEN_WIDTH / 2) - (ICON_SIZE / 2); // 图标X放在最中间

static const int TRIANGLE_BASE_Y = ICON_Y_POS - 5; // 三角形底部的y坐标
static const int TRIANGLE_PEAK_Y = TRIANGLE_BASE_Y - 20; // 三角形顶部的y坐标

int16_t display = INITIAL_X_OFFSET; // 当前显示的图标X坐标
uint8_t picture_flag = 0;           // 当前选中的菜单项索引

// 菜单项结构体
struct MenuItem {
    const char *name;              // 菜单项名称
    const uint16_t *image;         // 菜单项图标
    void (*action)();              // 菜单项动作函数指针
};

// 菜单项数组
const MenuItem menuItems[] = {
    {"Clock", Weather, &weatherMenu},
    {"Countdown", Countdown, &CountdownMenu},
    {"Alarm", alarm_img, &AlarmMenu}, 
    {"Pomodoro", tomato, &PomodoroMenu},
    {"Stopwatch", Timer, &StopwatchMenu},
    {"Music", Music, &BuzzerMenu},
    {"Music Lite", Music, &MusicMenuLite},
    {"Performance", Performance, &performanceMenu},
    {"Temperature",Temperature, &DS18B20Menu},
    {"Animation",LED, &AnimationMenu},
    {"Games", Games, &GamesMenu},
    {"LED", LED, &LEDMenu},
    {"ADC", ADC, &ADCMenu},
};
const uint8_t MENU_ITEM_COUNT = sizeof(menuItems) / sizeof(menuItems[0]); // 菜单项数量

// 菜单状态枚举
enum MenuState {
    MAIN_MENU,
    SUB_MENU,
    ANIMATING
};

// 全局状态变量
static MenuState current_state = MAIN_MENU;
static const uint8_t ANIMATION_STEPS = 12;

// 缓动函数
float easeOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float t_minus_1 = t - 1.0f;
    return 1.0f + c3 * t_minus_1 * t_minus_1 * t_minus_1 + c1 * t_minus_1 * t_minus_1;
}



// 绘制主菜单
void drawMenuIcons(int16_t offset) {
    // 清除图标和三角形绘制的区域
    menuSprite.fillRect(0, ICON_Y_POS, SCREEN_WIDTH, SCREEN_HEIGHT - ICON_Y_POS, TFT_BLACK);

    // 清除顶部文字区域
    menuSprite.fillRect(0, 0, SCREEN_WIDTH, 40, TFT_BLACK); // 从y=0到y=40清除

    // 绘制三角形指示器
    int16_t triangle_x = offset + (picture_flag * ICON_SPACING) + (ICON_SIZE / 2);
    menuSprite.fillTriangle(triangle_x, SCREEN_HEIGHT - 25, triangle_x - 12, SCREEN_HEIGHT - 5, triangle_x + 12, SCREEN_HEIGHT - 5, TFT_WHITE);

    // 绘制图标
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int16_t x = offset + (i * ICON_SPACING);
        if (x >= -ICON_SIZE && x < SCREEN_WIDTH) {
            menuSprite.pushImage(x, ICON_Y_POS, ICON_SIZE, ICON_SIZE, menuItems[i].image);
        }
    }

    // 绘制文字
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2); 
    menuSprite.setTextDatum(TC_DATUM); 
    menuSprite.drawString(menuItems[picture_flag].name, SCREEN_WIDTH / 2, 10); // 中心菜单项名称
    

    menuSprite.pushSprite(0, 0);
}

// 显示主菜单，在其他模块的函数退出的时候调用
void showMenuConfig() {
    tft.fillScreen(TFT_BLACK);
    drawMenuIcons(display);
}

// 主菜单导航，该函数在main.ino的loop()中被循环调用
void showMenu() {
    menuSprite.setTextFont(1);
    menuSprite.setTextSize(1); // 设置字体样式和大小，防止从其他模块退出忘记更改
    
    // 如果闹钟响了，则显示响铃屏幕
    if (g_alarm_is_ringing) {
        Alarm_ShowRingingScreen();
        showMenuConfig(); // 单击使得闹钟停止的时候，重绘菜单
        return; 
    }

    if (current_state != MAIN_MENU) return;
    
    int direction = readEncoder(); // 读取旋转编码器转动方向，顺时针返回1，逆时针返回-1，没动返回0
    if (direction != 0) {
        current_state = ANIMATING;
        
        if (direction == 1) { // 右转
            picture_flag = (picture_flag + 1) % MENU_ITEM_COUNT;
        } else if (direction == -1) { // 左转
            picture_flag = (picture_flag == 0) ? MENU_ITEM_COUNT - 1 : picture_flag - 1;
        }
        tone(BUZZER_PIN, 1000*(picture_flag + 1), 20);
        int16_t start_display = display; // 记录起始位置
        int16_t target_display = INITIAL_X_OFFSET - (picture_flag * ICON_SPACING);
        
        for (uint8_t i = 0; i <= ANIMATION_STEPS; i++) { // 循环动画帧
            float t = (float)i / ANIMATION_STEPS; // 计算进度，从0.0到1.0
            float eased_t = easeOutBack(t); // 应用缓动函数

            display = start_display + (target_display - start_display) * eased_t; // 计算插值位置

            drawMenuIcons(display);
            vTaskDelay(pdMS_TO_TICKS(20)); // 增加延迟，以获得更平滑的动画效果
        }
        
        display = target_display;
        drawMenuIcons(display);
        current_state = MAIN_MENU;
    }
    
    if (readButton()) {
        // 播放选择音效
        tone(BUZZER_PIN, 2000, 50);
        vTaskDelay(pdMS_TO_TICKS(50)); // 增加延迟，以播放完选择音效

        if (menuItems[picture_flag].action) {
            exitSubMenu = false;// 这个是MQTT收到exit修改的变量，在这里用不上 
            menuItems[picture_flag].action();
            showMenuConfig();
        }
    }
}

