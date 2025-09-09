# 游戏模块：休闲娱乐小游戏

## 一、引言

游戏模块为多功能时钟增添了休闲娱乐功能，提供了一些简单的小游戏，让用户在工作学习之余能够放松身心。本模块旨在展示设备的基本图形和交互能力，提供轻松愉快的游戏体验。请注意，本模块主要描述游戏的实现思路，而非完整的代码实现。

## 二、通用实现思路

### 2.1 游戏循环

所有游戏都将遵循一个基本的游戏循环结构：
1.  **输入处理**: 读取旋转编码器和按键的输入。
2.  **游戏状态更新**: 根据输入和游戏逻辑更新游戏世界的状态（例如，玩家位置、物体移动、分数等）。
3.  **渲染**: 将更新后的游戏状态绘制到屏幕上。
4.  **延时**: 控制游戏帧率，避免游戏运行过快。

### 2.2 用户交互

- **旋转编码器**: 主要用于控制游戏中的移动方向或选择。
- **按键**: 主要用于触发游戏动作（如跳跃、确认）或暂停/退出游戏。

### 2.3 图形绘制

- 利用 `TFT_eSPI` 库进行图形绘制，包括绘制矩形、圆形、线条和文本。
- 采用双缓冲技术（`TFT_eSprite`）来避免屏幕闪烁，确保游戏画面流畅。

### 2.4 游戏逻辑

- **碰撞检测**: 实现简单的碰撞检测算法，判断游戏元素之间是否发生接触。
- **得分机制**: 跟踪玩家得分。
- **游戏结束**: 定义游戏结束的条件，并在游戏结束后显示相应信息。

### 2.5 与其他模块的集成

- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，游戏能够及时退出，避免功能冲突。
- **子菜单退出**: `exitSubMenu` 标志用于从主菜单强制退出当前子菜单。

## 三、游戏实现思路

本模块的 `GamesMenu` 函数将作为游戏选择的入口。以下是可能包含在菜单中的几款小游戏的实现思路。

### 3.1 贪吃蛇 (Snake Game)

#### 游戏概念

玩家控制一条蛇在网格状的屏幕上移动，通过吃掉食物来增长身体。如果蛇头碰到墙壁或自己的身体，游戏结束。

#### 实现思路

- **游戏区域**: 将屏幕划分为一个二维网格，每个单元格代表一个游戏方块。
- **蛇的表示**: 使用一个链表或数组来存储蛇的每个身体节的坐标。每次移动时，在蛇头添加一个新的节，并在蛇尾移除一个节（除非吃到食物）。
- **食物生成**: 随机在网格的空闲位置生成食物。
- **移动控制**: 旋转编码器用于改变蛇的移动方向（上、下、左、右）。
- **碰撞检测**: 
    - **撞墙**: 检查蛇头是否超出屏幕边界。
    - **撞自己**: 检查蛇头坐标是否与蛇身体的任何一个节的坐标重合。
    - **吃食物**: 检查蛇头坐标是否与食物坐标重合。如果重合，蛇身增长，并生成新的食物。
- **得分**: 每吃到一个食物，分数增加。
- **游戏结束**: 撞墙或撞到自己时，显示“Game Over”和最终得分。

### 3.2 乒乓球 (Pong Game)

#### 游戏概念

一个简单的双人（或单人对AI）游戏。玩家控制一个挡板，击打一个在屏幕上反弹的球。目标是让球穿过对手的挡板。

#### 实现思路

- **挡板**: 两个矩形代表玩家的挡板。玩家的挡板通过旋转编码器控制其在Y轴上的移动。
- **球**: 一个圆形或小矩形代表球。球具有X和Y方向的速度，每次更新时根据速度改变位置。
- **碰撞检测**: 
    - **球与墙壁**: 球碰到上下边界时，Y方向速度反转。
    - **球与挡板**: 球碰到挡板时，X方向速度反转。可以根据球击中挡板的位置微调反弹角度或速度。
    - **得分**: 球穿过对手的挡板时，得分增加。
- **AI (可选)**: 如果是单人游戏，对手的挡板可以由简单的AI控制，例如让其Y坐标始终跟随球的Y坐标。
- **游戏结束**: 达到预设分数或时间限制时游戏结束。

### 3.3 飞扬的小鸟 (Flappy Bird - 简化版)

#### 游戏概念

玩家控制一只小鸟，通过点击屏幕让小鸟向上飞行，躲避不断出现的管道障碍物。如果小鸟碰到管道或地面，游戏结束。

#### 实现思路

- **小鸟**: 一个小矩形或圆形代表小鸟。小鸟的X坐标固定，只在Y轴上移动。它受重力影响持续下落。
- **跳跃**: 按键用于触发小鸟的跳跃动作，使其Y坐标向上移动一段距离。
- **管道**: 管道由两个矩形组成，中间留有空隙。管道从屏幕右侧生成，向左移动。管道的空隙高度和位置随机。
- **碰撞检测**: 
    - **小鸟与管道**: 检查小鸟的边界是否与任何管道的边界重叠。
    - **小鸟与地面/天空**: 检查小鸟是否超出屏幕的上下边界。
- **得分**: 每成功穿过一对管道，分数增加。
- **游戏结束**: 碰到管道或地面时，显示“Game Over”和最终得分。

## 四、代码展示 (概念性)

### `Games.cpp` (概念性代码)

```c++
#include "Games.h"
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include <TFT_eSPI.h>

// 假设的游戏入口函数，由菜单调用
void GamesMenu() {
    // 游戏选择界面
    int selectedGame = 0; // 0: Snake, 1: Pong, 2: Flappy Bird
    const char* gameNames[] = {"Snake", "Pong", "Flappy Bird"};
    const int NUM_GAMES = sizeof(gameNames) / sizeof(gameNames[0]);

    while (true) {
        if (g_alarm_is_ringing) { return; } // 闹钟响铃时退出

        // 绘制游戏选择列表
        menuSprite.fillScreen(TFT_BLACK);
        menuSprite.setTextDatum(MC_DATUM);
        menuSprite.setTextSize(2);
        menuSprite.setTextColor(TFT_WHITE);
        menuSprite.drawString("Select Game:", 120, 50);

        for (int i = 0; i < NUM_GAMES; i++) {
            if (i == selectedGame) {
                menuSprite.setTextColor(TFT_YELLOW);
            } else {
                menuSprite.setTextColor(TFT_WHITE);
            }
            menuSprite.drawString(gameNames[i], 120, 100 + i * 30);
        }
        menuSprite.pushSprite(0, 0);

        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            selectedGame = (selectedGame + encoderChange + NUM_GAMES) % NUM_GAMES;
            tone(BUZZER_PIN, 1000, 20);
        }

        if (readButton()) {
            tone(BUZZER_PIN, 1500, 50);
            // 根据选择进入游戏
            switch (selectedGame) {
                case 0: runSnakeGame(); break;
                case 1: runPongGame(); break;
                case 2: runFlappyBirdGame(); break;
            }
            // 游戏结束后返回选择界面
            break; 
        }
        if (readButtonLongPress()) { return; } // 长按退出游戏菜单
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// 概念性游戏函数
void runSnakeGame() {
    // 游戏初始化
    // 游戏循环 (输入处理 -> 状态更新 -> 渲染 -> 延时)
    // 游戏结束逻辑
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.drawString("Snake Game", 120, 100);
    menuSprite.drawString("Playing...", 120, 140);
    menuSprite.pushSprite(0,0);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Simulate game play
}

void runPongGame() {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.drawString("Pong Game", 120, 100);
    menuSprite.drawString("Playing...", 120, 140);
    menuSprite.pushSprite(0,0);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Simulate game play
}

void runFlappyBirdGame() {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.drawString("Flappy Bird", 120, 100);
    menuSprite.drawString("Playing...", 120, 140);
    menuSprite.pushSprite(0,0);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Simulate game play
}
```

## 五、总结与展望

游戏模块为多功能时钟提供了简单的娱乐功能，通过对经典小游戏的实现思路进行分析，展示了在资源有限的嵌入式设备上开发游戏的可能性。其核心在于高效的图形绘制、响应式输入处理和简洁的游戏逻辑。

未来的改进方向：
1.  **实现所有游戏**: 将上述概念性游戏完全实现，并进行优化。
2.  **高分榜**: 记录并显示每个游戏的最高分数。
3.  **音效与音乐**: 为游戏添加背景音乐和音效，提升游戏体验。
4.  **难度选择**: 允许用户选择游戏的难度级别。
5.  **更多游戏**: 探索实现更多适合小屏幕和简单交互的休闲游戏。

谢谢大家
