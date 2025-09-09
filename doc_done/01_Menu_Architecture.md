# 多功能时钟菜单系统

## 一、前言

菜单系统是多功能时钟的核心交互界面，它为用户提供了一个统一的入口来访问设备的所有功能，如时钟、倒计时、天气等。本模块的目标是实现一个响应快速、滚动平滑、易于扩展的横向菜单。用户通过旋转编码器在不同功能间切换，并通过旋转编码器的按键确认选择。

效果：

旋转编码器的具体内容可参考

[了解EC11旋转编码器，编写EC11旋转编码器驱动程序。 - 吃不了就兜着走 - 博客园](https://www.cnblogs.com/AChenWeiqiangA/p/12785276.html)

[【Arduino】用状态机精准处理EC11旋转编码器与按键-CSDN博客](https://blog.csdn.net/xiaomiao2006/article/details/151251185?fromshare=blogdetail&sharetype=blogdetail&sharerId=151251185&sharerefer=PC&sharesource=xiaomiao2006&sharefrom=from_link)

我在代码里面直接使用了写好的函数

## 二、实现思路

为了达成设计目标，我们首先解决了菜单项的数据结构问题。需求是需要一种统一的方式来管理每个菜单项的名称、图标和对应的执行逻辑，对此我们采用了`struct`数组结合函数指针的方案。这种“表驱动”的方法非常适合嵌入式项目，它将数据和行为绑定，同时保持了高度的模块化，可维护性很高。(更多关于此设计模式的讨论，可以参考 [【设计】三种常用的表驱动设计方法(附参考C代码)-CSDN博客](https://blog.csdn.net/qq_33471732/article/details/113787734))

接下来是处理UI渲染与闪烁问题。为了在单片机有限的刷新率下保证画面无闪烁，我们使用了`TFT_eSPI`库提供的精灵（Sprite）功能。它本质上是一个内存中的画布，所有绘制操作都先在内存中完成，然后一次性将最终画面传送到物理屏幕，这种双缓冲机制能完全避免视觉闪烁问题。可参考[C语言游戏开发闪屏解决办法--双缓冲技术_c语言如何让清屏打印不闪屏-CSDN博客](https://blog.csdn.net/NEFU_kadia/article/details/106211471)以及`TFT_eSPI`库的官方例程[TFT_eSPI/examples/Sprite at master · Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI/tree/master/examples/Sprite)

还有菜单图片的选择，首先是在屏幕上显示图片，可参考

[TFT_eSPI库显示自定义图片_tft.pushimage-CSDN博客](https://blog.csdn.net/weixin_47811387/article/details/135049847)

然后是图片的获取，可以在网站[iconfont-阿里巴巴矢量图标库](https://www.iconfont.cn/collections/index?spm=a313x.collections_index.i1.d33146d14.e01a3a81buM8Uq&type=3)上获取，点击获取PNG即可

最后，为了实现平滑的滚动动画，我们应用了缓动函数（Easing Function），具体为`easeOutBack`。相比于速度恒定的线性动画，缓动函数通过非线性的方式计算动画进程，可以模拟出带有弹性的物理效果，使交互体验更佳。[Easing Functions Cheat Sheet](https://easings.net/)网站上有不同缓动函数的效果

## 三、代码展示

`Menu.cpp`

```c++
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
            
            menuItems[picture_flag].action();
            showMenuConfig();// 从动作函数退出，重绘菜单
        }
    }
}
```

`main.ino`

十分简单，初始化，然后循环调用`showMenu()`即可

```c++
#include "Menu.h"
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
TFT_eSPI tft;
TFT_eSprite menuSprite;
void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    menuSprite.createSprite(239, 239);// 240,240不知道为啥屏幕会直接不显示，所以只好239,239
}

void loop() {
    showMenu();
    vTaskDelay(pdMS_TO_TICKS(15));
}
```



## 四、代码解读

以下是关键代码的实现分析。

### 菜单项的定义

```cpp
// 定义菜单项的数据结构
struct MenuItem {
    const char *name;              // 功能名称
    const uint16_t *image;         // 指向图标图像数据的指针
    void (*action)();              // 指向功能入口函数的指针
};

// 全局的菜单项数组，作为功能的“注册表”
const MenuItem menuItems[] = {
    {"Clock", Weather, &weatherMenu},
    {"Countdown", Countdown, &CountdownMenu},
    {"Alarm", alarm_img, &AlarmMenu},
    // ... 其他功能项
};
```

这段代码是系统模块化的基础。`MenuItem`结构体将一个功能的UI表示（`name`, `image`）和其逻辑入口（`action`）封装在一起。`menuItems`数组则集中管理了整个设备的所有可用功能。函数指针`action`是实现解耦的关键。菜单渲染和导航逻辑在执行用户选择时，只需调用`menuItems[index].action()`即可，它不关心也不需要知道`weatherMenu`或`CountdownMenu`函数的具体内容。这种设计使得各个功能模块可以被独立开发和测试。

### UI渲染与视口裁剪 

```cpp
void drawMenuIcons(int16_t offset) {
    // ... 清理Sprite画布 ...

    // 遍历所有菜单项，计算它们在滚动条上的位置
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int16_t x = offset + (i * ICON_SPACING);
        
        // 视口裁剪：只绘制在屏幕可视范围内的图标
        if (x >= -ICON_SIZE && x < SCREEN_WIDTH) {
            menuSprite.pushImage(x, ICON_Y_POS, ICON_SIZE, ICON_SIZE, menuItems[i].image);
        }
    }
    
    // ... 绘制标题文字和选中指示器 ...
    
    // 将Sprite画布内容一次性推送到屏幕
    menuSprite.pushSprite(0, 0);
}
```

`drawMenuIcons`函数负责每一帧的界面渲染。它根据一个全局的`offset`（菜单滚动偏移量）来计算并绘制所有元素。其中，`if (x >= -ICON_SIZE && x < SCREEN_WIDTH)`是一个重要的性能优化，通常被称为视口裁剪。它通过简单的坐标判断，跳过了对屏幕外不可见元素的绘制调用，有效降低了在快速滚动等场景下的CPU负载。

### 动画循环与插值

```cpp
if (direction != 0) { // 检测到编码器输入
    // ... 更新目标索引 picture_flag ...
    
    int16_t start_display = display; // 动画起始偏移量
    int16_t target_display = INITIAL_X_OFFSET - (picture_flag * ICON_SPACING); // 动画目标偏移量
    
    // 通过一个for循环生成动画的每一帧
    for (uint8_t i = 0; i <= ANIMATION_STEPS; i++) {
        float t = (float)i / ANIMATION_STEPS; 
        float eased_t = easeOutBack(t); 
        display = start_display + (target_display - start_display) * eased_t;
        drawMenuIcons(display); 
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
    
    display = target_display; // 修正最终位置，确保精确
    drawMenuIcons(display);
}
```

这段逻辑是实现平滑动画的核心。当检测到用户输入后，程序并非直接将位置设置为目标点，而是通过一个循环逐步过渡。动画生成的步骤清晰而高效：循环为每一帧计算一个从0.0到1.0的线性进度`t`；接着，这个线性进度被传入`easeOutBack`函数进行转换，得到一个非线性的进度`eased_t`；随后，程序使用标准的线性插值（Lerp）公式，并以`eased_t`作为进度，计算出当前帧的准确`display`值；最后，调用`drawMenuIcons`来渲染这一帧的画面，并通过短暂延时来控制动画的播放速度。这个过程在短时间内快速重复，就构成了我们所见的连续动画。

## 四、效果展示

最终实现的菜单界面滚动流畅，响应迅速。双缓冲机制有效避免了任何视觉闪烁。`easeOutBack`缓动函数的运用使得UI动画显得精致且不失物理感。此外，在驱动层面对编码器和按键的抖动进行了处理，保证了硬件输入的稳定可靠。



## 五、总结与展望

该菜单系统通过表驱动设计、双缓冲渲染和缓动动画三大核心技术，成功构建了一个用户体验良好且易于维护的UI框架。

未来的可扩展方向包括：为设置等复杂模块构建层级更深的二级菜单；根据系统状态（如是否联网）动态显示或隐藏某些菜单项；或者将颜色、字体等UI元素配置化，以支持一键换肤。

谢谢大家
