#include <TFT_eSPI.h>
#include <RotaryEncoder.h>
#include <math.h>

// 全局变量
TFT_eSPI tft = TFT_eSPI();         // TFT显示屏对象
TFT_eSprite menuSprite = TFT_eSprite(&tft);
int16_t display = 48;              // 图标初始x偏移
uint8_t picture_flag = 0;          // 当前选中的菜单项索引
const int icon_size = 32;          // 图标尺寸（32x32像素）

// 图像数组（在外部定义，16位RGB565格式，32x32像素）

// 菜单项结构
struct MenuItem {
    const char *name;              // 菜单项名称
    const uint16_t *image;         // 菜单项图像
};

// 菜单项数组
const MenuItem menuItems[] = {
    {"Game", game},
    {"Weather", Weather},
    {"Perf", performance}
};
const uint8_t MENU_ITEM_COUNT = sizeof(menuItems) / sizeof(menuItems[0]); // 菜单项数量

// 菜单状态枚举
enum MenuState {
    MAIN_MENU,    // 主菜单状态
    SUB_MENU,     // 子菜单状态
    ANIMATING     // 动画过渡状态
};

// 全局状态变量
static MenuState current_state = MAIN_MENU;    // 当前菜单状态
static int16_t menu_display = 10;              // 菜单文字y坐标
static int16_t menu_display_trg = 74;          // 目标文字y坐标
static const uint8_t ANIMATION_STEPS = 12;     // 动画步数
static const uint8_t EASING_FACTOR = 8;        // 缓动因子

// 缓动函数（二次缓出）
float easeOutQuad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// 动画平滑移动
void ui_run_easing(int16_t *current, int16_t target, uint8_t steps) {
    if (*current == target) return;
    float t = (float)(ANIMATION_STEPS - steps) / ANIMATION_STEPS;
    float eased = easeOutQuad(t);
    int16_t delta = target - *current;
    *current += (int16_t)(delta * eased / steps);
    if (abs(*current - target) < 2) {
        *current = target;
    }
}

// 绘制主菜单（已修改为使用Sprite缓冲）
void drawMenuIcons(int16_t offset) {
    // 1. 将Sprite完全填充为黑色，实现清屏效果
    menuSprite.fillSprite(TFT_BLACK);
    
    // 2. 绘制三角形指示器 (所有Y坐标都已转换为Sprite的内部坐标)
    int16_t triangle_x = offset + (picture_flag * 48) + (icon_size / 2);
    // 原屏幕Y坐标 46, 60 -> Sprite内部Y坐标 41, 55
    menuSprite.fillTriangle(triangle_x, 55, triangle_x - 10, 41, triangle_x + 10, 41, TFT_WHITE);
    
    // 3. 绘制菜单项图像
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int16_t x = offset + (i * 48);
        if (x >= -icon_size && x < 128) {
            // 原屏幕Y坐标 16 -> Sprite内部Y坐标 11
            menuSprite.pushImage(x, 11, 32, 32, menuItems[i].image);
        }
    }

    // 4. 绘制文字
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(1);
    menuSprite.setTextDatum(TL_DATUM);
    // 原屏幕Y坐标 5 -> Sprite内部Y坐标 0
    menuSprite.drawString("MENU:", 52, 0);
    menuSprite.drawString(menuItems[picture_flag].name, 82, 0);
    
    // 5. 将绘制完成的Sprite一次性推送到屏幕的(0, 5)位置
    menuSprite.pushSprite(0, 5);
}

// 显示主菜单（替换旧函数）
void showMenuConfig() {
    tft.fillScreen(TFT_BLACK); // 初始清屏
    drawMenuIcons(display);    // 使用新的绘制函数
}

// 动画过渡（进入或退出子菜单）
// 动画过渡（进入或退出子菜单）- 修正版
void animateMenuTransition(const char *title, bool entering) {
    current_state = ANIMATING; // 设置为动画状态
    int16_t start_y = entering ? 5 : 74;   // 文本动画的起始Y坐标
    int16_t target_y = entering ? 74 : 5;    // 文本动画的目标Y坐标

    int16_t animated_text_y = start_y; // 用于驱动文本动画的局部变量

    for (uint8_t step = ANIMATION_STEPS; step > 0; step--) {
        // 1. 清理动画所需的屏幕区域
        tft.fillRect(0, 5, 128, 80, TFT_BLACK);

        // 2. 手动绘制图标和三角形，让它们在动画期间保持静止
        //    (我们不调用drawMenuIcons，因为它用于绘制完整的静态主菜单)
        
        // 绘制图标
        for (int i = 0; i < MENU_ITEM_COUNT; i++) {
            int16_t x = display + (i * 48);
            if (x >= -icon_size && x < 128) {
                // 使用固定的Y坐标(16)来绘制图标
                tft.pushImage(x, 16, 32, 32, menuItems[i].image);
            }
        }
        
        // 绘制三角形
        int16_t triangle_x = display + (picture_flag * 48) + (icon_size / 2);
        // 使用固定的Y坐标(46, 60)来绘制三角形
        tft.fillTriangle(triangle_x, 60, triangle_x - 10, 46, triangle_x + 10, 46, TFT_WHITE);

        // 3. 在动画的Y坐标处绘制文本
        tft.drawString("MENU:", 52, animated_text_y);
        tft.drawString(title, 82, animated_text_y);

        // 4. 更新下一个动画帧的Y坐标
        ui_run_easing(&animated_text_y, target_y, step);
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    // 设置最终状态
    current_state = entering ? SUB_MENU : MAIN_MENU;
}


// 显示子菜单（图像）
void showSubMenu(const char *title, const uint16_t *image) {
    animateMenuTransition(title, true);
    
    tft.fillScreen(TFT_BLACK); // 清屏
    // 居中显示32x32图像
    tft.pushImage((tft.width() - 32) / 2, (tft.height() - 32) / 2, 32, 32, image);
    
    while (current_state == SUB_MENU) {
        if (readButton()) { // 检测按钮按下
            animateMenuTransition(title, false);
            showMenuConfig();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// 子菜单实现
void gameMenu() {
    showSubMenu("GAME", menuItems[0].image);
}

void weatherMenu() {
    showSubMenu("WEATHER", menuItems[1].image);
}

void performanceMenu() {
    showSubMenu("PERFORMANCE", menuItems[2].image);
}

// 主菜单导航
// 主菜单导航（替换旧函数）
void showMenu() {
    if (current_state != MAIN_MENU) return;
    
    int direction = readEncoder(); // 读取编码器方向
    if (direction != 0) {
        current_state = ANIMATING; // 设置为动画状态
        
        if (direction == 1) { // 向右
            picture_flag = (picture_flag + 1) % MENU_ITEM_COUNT;
        } else if (direction == -1) { // 向左
            picture_flag = (picture_flag == 0) ? MENU_ITEM_COUNT - 1 : picture_flag - 1;
        }
        
        int16_t target_display = 48 - (picture_flag * 48);
        
        for (uint8_t step = ANIMATION_STEPS; step > 0; step--) {
            ui_run_easing(&display, target_display, step);
            // 每一帧动画都调用新的Sprite绘制函数
            drawMenuIcons(display);
            vTaskDelay(pdMS_TO_TICKS(15));
        }
        
        display = target_display; // 确保动画结束时位置精确
        drawMenuIcons(display);   // 绘制最终状态
        current_state = MAIN_MENU;
    }
    
    if (readButton()) { // 检测按钮按下
        switch (picture_flag) {
            case 0: gameMenu(); break;
            case 1: weatherMenu(); break;
            case 2: performanceMenu(); break;
        }
    }
}

// Arduino初始化
void setup() {
    initRotaryEncoder(); // 初始化旋转编码器
    tft.init(); // 初始化TFT显示屏
    tft.setRotation(1); // 设置屏幕旋转方向

    menuSprite.createSprite(128, 65);

    tft.fillScreen(TFT_BLACK); // 清屏
    showMenuConfig(); // 显示主菜单
}

// Arduino主循环
void loop() {
    showMenu(); // 处理主菜单导航
    vTaskDelay(pdMS_TO_TICKS(15)); // 延时15ms
}