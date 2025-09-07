# 用状态机精准处理EC11旋转编码器与按键

## 一、引言

在嵌入式人机交互领域，旋转编码器因其无限的旋转范围**、**清晰的段落感以及集成按压功能，成为了许多项目的输入首选。

这里，我们将深入剖析一个用于多功能时钟项目的旋转编码器驱动模块。这段代码不仅稳定可靠地读取旋转方向，更实现了单击、带视觉反馈的长按、以及双击功能。

## 二、增量式编码器EC11

### 2.1 概述

EC11的旋转与按下不同，不能当作一个简单的按键来进行处理，它是一种增量式旋转编码器，在原理图中，可以看到ABC三个输出通道，其中C相接入GND，AB两相输出正交信号，相位差为90°。

### 2.2 电路连接

![编码器驱动](C:\Users\xiaom\Desktop\暑假笔记\blog\编码器驱动.png)

可参考[EC11旋转编码器实验 | 嘉立创EDA教育与开源文档中心](https://wiki.lceda.cn/zh-hans/course-projects/microcontroller/ch32-multimeter/ec11coder-test.html)

这里面加入了滤波电容，而我为了简单，把编码器的接口直接连到了单片机的IO口，也可正常使用

### 2.3 输出信号

![编码器](C:\Users\xiaom\Desktop\暑假笔记\blog\编码器.png)

旋转编码器内部相当于两个开关，其CLK和DT引脚会输出一种叫做格雷码的序列。格雷码的特点是任意两个相邻的数字其二进制表示只有一位不同，这使得它在旋转过程中不会产生严重的误码。

一次完整的旋转（一格）会产生4个状态。假设从静止开始：

顺时针旋转（CW）一格的典型序列：

1. 状态 `11` (CLK=1, DT=1) -> 起始状态
2. 状态 `10` (CLK=1, DT=0) -> CLK保持高，DT变为低
3. 状态 `00` (CLK=0, DT=0) -> CLK变为低，DT保持低
4. 状态 `01` (CLK=0, DT=1) -> CLK保持低，DT变为高
5. 状态 `11` (CLK=1, DT=1) -> 回到起始状态，完成一格

逆时针旋转（CCW）一格的典型序列：

1. 状态 `11` (CLK=1, DT=1) -> 起始状态
2. 状态 `01` (CLK=0, DT=1) -> CLK变为低，DT保持高
3. 状态 `00` (CLK=0, DT=0) -> CLK保持低，DT变为低
4. 状态 `10` (CLK=1, DT=0) -> CLK变为高，DT保持低
5. 状态 `11` (CLK=1, DT=1) -> 回到起始状态，完成一格

## 三、模块概述

本模块的核心功能是驱动一个标准的增量式旋转编码器，并读取其旋转方向和集成按键的状态。它实现了：

1. 旋转检测：确识别顺时针/逆时针旋转，并处理抖动。
2. 单击检测：识别用户的轻按操作。
3. 长按检测：识别用户的长按操作（如2秒），并在屏幕上提供可视化进度条反馈，提升用户体验。
4. 双击检测：识别用户在短时间内快速的连续两次单击操作，用于触发快捷功能。

## 四、实现思路

### 4.1 旋转检测

#### 法一：状态变化法

这个方法很简单，在CLK引脚的电平发生变化时（上升沿或下降沿），去检查DT引脚的电平状态。

当 CLK从高变低（下降沿） 时：

- 如果 **DT为低电平**，则是**顺时针**旋转。

- 如果 **DT为高电平**，则是**逆时针**旋转。

    ```c++
    int readEncoder() {
      static int lastCLK_State = HIGH; // 静态变量，保存CLK引脚上一次的状态
      int currentCLK_State = digitalRead(ENCODER_CLK); // 读取CLK引脚当前状态
      int delta = 0; // 返回值，0=无变化，1=顺时针，-1=逆时针
    
      // 检查CLK引脚的状态是否发生了变化（即出现了边沿）
      if (currentCLK_State != lastCLK_State) {
        // 简单软件消抖：如果状态变化了，等待一小段时间再确认，避免抖动
        delayMicroseconds(500); // 等待500微秒，这是一个常用的消抖时间
    
        // 再次读取CLK和DT引脚，确保状态稳定
        currentCLK_State = digitalRead(ENCODER_CLK);
        int currentDT_State = digitalRead(ENCODER_DT);
    
        #if SWAP_CLK_DT
          // 如果配置了交换引脚，则交换CLK和DT的角色
          int temp = currentCLK_State;
          currentCLK_State = currentDT_State;
          currentDT_State = temp;
        #endif
    
        // 判断逻辑：只有在CLK为 LOW 时判断DT的状态，方向才最准确
        // 你也可以在CLK为 HIGH 时判断，但需要与下面的判断条件相反
        if (currentCLK_State == LOW) {
          if (currentDT_State == LOW) {
            // CLK为低时DT也为低 -> 顺时针旋转
            delta = 1;
            encoderPos++;
          } else {
            // CLK为低时DT为高 -> 逆时针旋转
            delta = -1;
            encoderPos--;
          }
        }
        // 更新上一次的CLK状态，为下一次判断做准备
        lastCLK_State = currentCLK_State;
      }
    
      return delta;
    }
    ```


#### 法二：状态机查表法 

##### 状态编码与存储

```cpp
uint8_t currentEncoded = (clk << 1) | dt; // 组合新状态
```

这行代码将两个引脚的状态组合成一个2位的数字。

- 例如：`CLK=1, DT=1` -> `(1 << 1) | 1 = 2 | 1 = 3` (二进制 `0b11`)
- `CLK=1, DT=0` -> `(1 << 1) | 0 = 2 | 0 = 2` (二进制 `0b10`)
- 这样，`lastEncoded` 和 `currentEncoded` 就可以各表示4种状态（0, 1, 2, 3）。

##### 查找表

```cpp
static const int8_t transitionTable[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
```

这个数组包含了所有16种可能的状态转换结果。**索引的计算方式是：`(lastEncoded << 2) | currentEncoded`**。

- `lastEncoded` 是旧状态（0-3），左移2位后变成高2位（0, 4, 8, 12）。
- `currentEncoded` 是新状态（0-3），作为低2位。
- 这样组合出的索引范围是0到15，正好对应数组的16个元素。

**这个表是如何工作的？**
让我们用顺时针序列来验证：

1. 从 `11` (3) 转到 `10` (2)：索引 = `(3 << 2) | 2 = 12 | 2 = 14` -> `transitionTable[14] = 1` (顺时针)
2. 从 `10` (2) 转到 `00` (0)：索引 = `(2 << 2) | 0 = 8 | 0 = 8` -> `transitionTable[8] = 1` (顺时针)
3. 从 `00` (0) 转到 `01` (1)：索引 = `(0 << 2) | 1 = 0 | 1 = 1` -> `transitionTable[1] = 1` (顺时针)
4. 从 `01` (1) 转到 `11` (3)：索引 = `(1 << 2) | 3 = 4 | 3 = 7` -> `transitionTable[7] = 1` (顺时针)

逆时针序列也会产生连续的 `-1`。而无效的跳变（如由抖动引起）在表中对应的值大多是 `0`。

##### 累加器与滤波

```cpp
deltaSum += deltaValue; // 来自查表的值(-1, 0, 1)

if (deltaSum >= 2) {
    ... // 触发顺时针
    deltaSum = 0; // 重置
} else if (deltaSum <= -2) {
    ... // 触发逆时针
    deltaSum = 0; // 重置
}
```

### 4.2 按键检测：状态机与时间戳

按键处理是一个复杂的状态机问题。我们需要通过记录按下时刻的时间戳和状态转换来清晰区分**单击**、**长按**和**双击**。

**关键难点在于如何无歧义地区分三者？**
我们的策略是使用一个四状态的状态机(`ButtonState`)：

- **IDLE (空闲)**：等待第一次按下。
- **PRESSED (第一次按下)**：记录按下时间，等待释放。在此状态下，`readButtonLongPress()`函数会检查是否达到长按阈值。
- **RELEASED (第一次释放)**：此时启动一个定时器（`doubleClickTimeout`），在此窗口内：
    - 如果用户再次按下，则进入`DOUBLE_PRESSED`状态。
    - 如果定时器超时，则判定这是一次**单击**，并触发单击事件。
- **DOUBLE_PRESSED (第二次按下)**：等待第二次释放，一旦释放，立即标记**双击**事件。

状态机工作流程如下图所示

```
      [BTN_IDLE]
          | (按下)
          v
      [BTN_PRESSED] <---[长按发生]---> (触发长按)
          | (释放)
          v
      [BTN_RELEASED] ---(超时)---> [BTN_IDLE] & 触发单击
          | (再次按下)
          v
      [BTN_DOUBLE_PRESSED]
          | (释放)
          v
      [BTN_IDLE] & 标记双击
```



这种设计确保了长按优先于双击（因为长按在第一次按下期间就已判定），并且单击是双击的超时后备，逻辑清晰，无歧义。

为消除长按时的焦虑，代码在按住1秒后（`progressBarStartTime`）开始在TFT屏幕上绘制一个逐渐增长的进度条，直观地提示还需按住多久才能触发长按功能。

## 五、代码展示

`RotaryEncoder.cpp`

```c++
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>

//旋转编码器状态变量
static volatile int encoderPos = 0;      // 编码器的累计绝对位置（可能为正或负）
static uint8_t lastEncoded = 0;          // 上一次读取的CLK和DT引脚状态组合（用于状态比较）
static int deltaSum = 0;                 // 微小变化的累加器（用于滤波，确保一次有效的转动才触发一次变化）
static unsigned long lastEncoderTime = 0; // 上次读取编码器的时间戳（用于软件消抖）

// --- 按键状态机变量 ---
// 状态定义：使用枚举列出按键可能处于的所有状态
enum ButtonState {
  BTN_IDLE,           // 空闲状态：无按键动作，等待第一次按下
  BTN_PRESSED,        // 第一次按下：等待释放或转为长按
  BTN_RELEASED,       // 第一次按下后已释放：等待第二次按下（以构成双击）或超时（则判定为单击）
  BTN_DOUBLE_PRESSED  // 第二次按下：等待释放以完成双击确认
};
static ButtonState buttonFSM = BTN_IDLE; // 按键状态机当前状态，初始化为空闲状态

// 时间戳与标志位
static unsigned long buttonPressStartTime = 0; // 记录当前按下状态的开始时间（毫秒），用于计算按压时长以检测长按
static unsigned long firstReleaseTime = 0;     // 记录第一次释放的时间戳（毫秒），用于计算双击的时间窗口
static bool longPressHandled = false;          // 长按已处理标志，防止长按事件被重复触发
static bool clickHandled = false;              // 单击已处理标志，防止在双击过程中，第一次按下释放后被误判为单独的单击
static bool doubleClickDetected = false;       // 双击已发生标志，由状态机在检测到双击时设置，由readDoubleClick()函数读取并清除

// --- 参数配置 ---
#define SWAP_CLK_DT 0 // 是否交换CLK和DT引脚的定义（0：不交换；1：交换）。用于调整旋转方向。
static const unsigned long longPressThreshold = 2000;     // 长按判定阈值(ms)，按压超过此时间视为长按
static const unsigned long progressBarStartTime = 1000;   // 进度条开始显示的时间(ms)，按压达到此时间后开始显示长按进度条
static const unsigned long doubleClickTimeout = 300;      // 双击时间窗口(ms)，从第一次释放开始计时，在此时间内未等到第二次按下则判定为单击

// 进度条尺寸参数（在屏幕上的位置和大小）
static const int BAR_X = 50;      // 进度条左上角X坐标
static const int BAR_Y = 30;      // 进度条左上角Y坐标
static const int BAR_WIDTH = 140; // 进度条宽度
static const int BAR_HEIGHT = 10; // 进度条高度

// 初始化旋转编码器引脚
void initRotaryEncoder() {
  // 设置CLK和DT引脚为输入上拉模式（默认高电平，旋转编码器将其拉低）
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  // 设置按键(SW)引脚为输入上拉模式（按下时引脚被拉低）
  pinMode(ENCODER_SW, INPUT_PULLUP);
  // 初始化lastEncoded变量，记录编码器初始状态
  // 将CLK和DT的状态组合成一个2位的数: (CLK << 1) | DT
  lastEncoded = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);
  Serial.println("Rotary Encoder Initialized."); // 串口打印初始化完成信息
}

// ======查表法读取编码器旋转方向=====
// 返回值: -1 (逆时针), 0 (无变化), 1 (顺时针)
// int readEncoder() {
//   unsigned long currentTime = millis(); // 获取当前时间
//   // 简单软件消抖：如果距离上次读取时间太短（<2ms），则忽略此次读取，直接返回0
//   if (currentTime - lastEncoderTime < 2) {
//     return 0;
//   }
//   lastEncoderTime = currentTime; // 更新上次读取时间

//   // 读取CLK和DT引脚的当前电平状态
//   int clk = digitalRead(ENCODER_CLK);
//   int dt = digitalRead(ENCODER_DT);
//   // 如果配置了交换引脚，则交换clk和dt的值（用于调整旋转方向感知）
//   #if SWAP_CLK_DT
//     int temp = clk; clk = dt; dt = temp;
//   #endif

//   // 将当前CLK和DT的状态组合成一个2位的数，与lastEncoded格式相同
//   uint8_t currentEncoded = (clk << 1) | dt;
//   int delta = 0; // 本次函数调用的返回值，默认为0（无变化）

//   // 只有当编码器状态发生变化时才进行处理
//   if (currentEncoded != lastEncoded) {
//     // 状态查找表：根据旧状态（高2位）和新状态（低2位）索引出方向变化量（-1, 0, 1）
//     // 有效的状态序列会产生±1，无效的抖动序列会产生0
//     static const int8_t transitionTable[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
//     // 计算索引：旧状态左移2位（作为高2位）与当前状态（作为低2位）进行或运算
//     int8_t deltaValue = transitionTable[(lastEncoded << 2) | currentEncoded];
//     // 将变化量累加到deltaSum中（一种滤波方式，累计足够的变化才算一次有效转动）
//     deltaSum += deltaValue;

//     // 如果累加和达到2，表示一个有效的顺时针步进
//     if (deltaSum >= 2) {
//       encoderPos++;   // 绝对位置增加
//       delta = 1;      // 设置返回值为1（顺时针）
//       deltaSum = 0;   // 重置累加器
//     } else if (deltaSum <= -2) { // 如果累加和达到-2，表示一个有效的逆时针步进
//       encoderPos--;   // 绝对位置减少
//       delta = -1;     // 设置返回值为-1（逆时针）
//       deltaSum = 0;   // 重置累加器
//     }
//   }
//   // 更新上一次的编码器状态，为下一次读取做准备
//   lastEncoded = currentEncoded;
//   // 返回本次检测到的方向变化（0，1，-1）
//   return delta;
// }
// ======状态变化法读取编码器旋转方向=====
int readEncoder() {
  static int lastCLK_State = HIGH; // 静态变量，保存CLK引脚上一次的状态
  int currentCLK_State = digitalRead(ENCODER_CLK); // 读取CLK引脚当前状态
  int delta = 0; // 返回值，0=无变化，1=顺时针，-1=逆时针

  // 检查CLK引脚的状态是否发生了变化（即出现了边沿）
  if (currentCLK_State != lastCLK_State) {
    // 简单软件消抖：如果状态变化了，等待一小段时间再确认，避免抖动
    delayMicroseconds(500); // 等待500微秒，这是一个常用的消抖时间

    // 再次读取CLK和DT引脚，确保状态稳定
    currentCLK_State = digitalRead(ENCODER_CLK);
    int currentDT_State = digitalRead(ENCODER_DT);

    #if SWAP_CLK_DT
      // 如果配置了交换引脚，则交换CLK和DT的角色
      int temp = currentCLK_State;
      currentCLK_State = currentDT_State;
      currentDT_State = temp;
    #endif

    // 判断逻辑：只有在CLK为 LOW 时判断DT的状态，方向才最准确
    // 你也可以在CLK为 HIGH 时判断，但需要与下面的判断条件相反
    if (currentCLK_State == LOW) {
      if (currentDT_State == LOW) {
        // CLK为低时DT也为低 -> 顺时针旋转
        delta = 1;
        encoderPos++;
      } else {
        // CLK为低时DT为高 -> 逆时针旋转
        delta = -1;
        encoderPos--;
      }
    }
    // 更新上一次的CLK状态，为下一次判断做准备
    lastCLK_State = currentCLK_State;
  }

  return delta;
}
// 核心按键状态机，必须被频繁循环调用以检测状态变化
// 返回值: 1 表示检测到一个单击事件（仅在状态机判定单击完成后返回一次），其他情况返回0
int readButton() {
  int currentButtonState = digitalRead(ENCODER_SW); // 读取按键当前电平（LOW:按下, HIGH:释放）
  bool singleClickTriggered = false; // 本次调用是否触发了单击事件的标志
  unsigned long currentTime = millis(); // 获取当前时间

  // 状态机逻辑：根据当前状态和输入（引脚电平）进行状态转移
  switch (buttonFSM) {
    case BTN_IDLE: // 空闲状态
      if (currentButtonState == LOW) { // 如果检测到按键被按下
        buttonFSM = BTN_PRESSED; // 转移到“按下”状态
        buttonPressStartTime = currentTime; // 记录按下开始的时间
        longPressHandled = false;   // 重置长按处理标志
        clickHandled = false;       // 重置单击处理标志
        doubleClickDetected = false; // 重置双击检测标志
      }
      break;

    case BTN_PRESSED: // 按下状态
      if (currentButtonState == HIGH) { // 如果按键被释放
        firstReleaseTime = currentTime; // 记录第一次释放的时间
        buttonFSM = BTN_RELEASED; // 转移到“已释放”状态，等待可能的第二次按下
      }
      // 注意：长按检测逻辑在另一个函数 readButtonLongPress() 中处理，它会在PRESSED状态下检查时间
      break;

    case BTN_RELEASED: // 第一次按下后释放的状态
      // 检查是否超过了双击等待时间（超时）
      if (currentTime - firstReleaseTime > doubleClickTimeout) {
        // 如果超时，且单击事件尚未被标记为已处理（例如未被长按处理标志覆盖）
        if (!clickHandled) {
          singleClickTriggered = true; // 触发单击事件
          clickHandled = true;         // 标记单击已处理，防止重复触发
        }
        buttonFSM = BTN_IDLE; // 状态回归空闲，等待下一次按键
      }
      // 在超时检查之后，检查是否发生了第二次按下
      if (currentButtonState == LOW) {
        buttonFSM = BTN_DOUBLE_PRESSED; // 转移到“第二次按下”状态
        buttonPressStartTime = currentTime; // 重置按下时间戳，防止此次按下被计入长按时间
      }
      break;

    case BTN_DOUBLE_PRESSED: // 第二次按下状态
      if (currentButtonState == HIGH) { // 如果第二次按下后释放了
        doubleClickDetected = true;     // 标记双击事件已经发生
        buttonFSM = BTN_IDLE;           // 状态回归空闲
      }
      break;
  }
  // 返回是否触发了单击事件（只在从RELEASED状态超时转回IDLE时返回一次1）
  return singleClickTriggered;
}

// 检测长按事件，需要在循环中频繁调用
// 返回值: true 如果检测到一个长按事件（仅返回一次），否则 false
bool readButtonLongPress() {
  bool triggered = false; // 本次调用是否触发了长按事件的标志

  // 仅在状态为“按下”(BTN_PRESSED) 且长按事件尚未被处理过的状态下进行检测
  if (buttonFSM == BTN_PRESSED && !longPressHandled) {
    unsigned long currentHoldTime = millis() - buttonPressStartTime; // 计算当前已按压的时间

    // 显示进度条逻辑：如果按压时间超过了进度条开始显示的时间阈值
    if (currentHoldTime >= progressBarStartTime) {
        // 计算进度比例 (0.0 到 1.0)
        // 从progressBarStartTime开始计算，到longPressThreshold达到100%
        float progress = (float)(currentHoldTime - progressBarStartTime) / (longPressThreshold - progressBarStartTime);
        progress = (progress > 1.0) ? 1.0 : progress; // 确保进度不超过1.0
        // 在menuSprite上绘制进度条外框和填充
        menuSprite.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_WHITE); // 绘制白色外框
        menuSprite.fillRect(BAR_X + 2, BAR_Y + 2, (int)((BAR_WIDTH - 4) * progress), BAR_HEIGHT - 4, TFT_BLUE); // 用蓝色填充内部，宽度随进度增加
        menuSprite.pushSprite(0,0); // 将menuSprite的内容推送到屏幕上显示
    }

    // 长按触发判断：如果按压时间超过了长按阈值
    if (currentHoldTime >= longPressThreshold) {
        triggered = true;        // 标记触发了长按事件
        longPressHandled = true; // 设置长按已处理标志，防止重复触发
        clickHandled = true;     // 同时标记单击已处理，因为长按已经发生，后续的释放不应再触发单击事件
    }
  }

  // 进度条清理逻辑：当按键是释放状态（HIGH）
  if (digitalRead(ENCODER_SW) == HIGH) {
      // 并且之前有按下记录，且按下时间曾经超过显示进度条的阈值
      if (buttonPressStartTime != 0 && (millis() - buttonPressStartTime) > progressBarStartTime) {
          // 则用黑色填充清除进度条区域
          menuSprite.fillRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_BLACK);
          menuSprite.pushSprite(0,0); // 更新屏幕显示
      }
  }

  return triggered; // 返回是否检测到长按
}

// 检测双击事件
// 返回值: true 如果检测到一个双击事件（仅返回一次），否则 false
bool readDoubleClick() {
  if (doubleClickDetected) { // 检查双击标志是否被状态机设置
    doubleClickDetected = false; // 读取后立即清除标志，确保事件只被报告一次
    return true;
  }
  return false;
}
```

`RotaryEncoder.h`

```c++
#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H
#include <TFT_eSPI.h>
#include <Arduino.h>

// 定义编码器引脚，根据实际硬件连接修改
#define ENCODER_CLK 1
#define ENCODER_DT  0
#define ENCODER_SW  7
extern TFT_eSPI tft;
extern TFT_eSprite menuSprite;
// 初始化函数
void initRotaryEncoder();

// 读取旋转方向
int readEncoder();

// 读取按键状态（已升级为状态机核心）
int readButton();

// 读取长按事件
bool readButtonLongPress();

// 读取双击事件
bool readDoubleClick();

#endif
```

`main.ino`

```c++
#include "RotaryEncoder.h"

void setup() {
  initRotaryEncoder();
  // ... 其他初始化代码
}

void loop() {
  // 1. 必须持续调用readButton()以更新状态机
  int clickEvent = readButton();

  // 2. 检查各种事件
  int rotation = readEncoder();
  if (rotation != 0) {
    Serial.println(rotation == 1 ? "CW" : "CCW");
    // 处理旋转...
  }

  if (clickEvent) {
    Serial.println("Single Click");
    // 处理单击...
  }

  if (readButtonLongPress()) {
    Serial.println("Long Press");
    // 处理长按...
  }

  if (readDoubleClick()) {
    Serial.println("Double Click!");
    // 处理双击...
  }

  // ... 其他主循环代码
  delay(1); // 短暂延时，避免循环过快
}
```

因为我的板子串口打印出问题了，就换成了这个`main.ino`测试，但本质都是循环调用这几个函数

```c++
#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite menuSprite = TFT_eSprite(&tft);

// 定义显示位置
#define TITLE_X 10
#define TITLE_Y 10
#define EVENT_X 10
#define EVENT_Y 40
#define HISTORY_X 10
#define HISTORY_Y 70
#define LINE_HEIGHT 20

// 显示持续时间（毫秒）
#define DISPLAY_DURATION 3000

// 历史记录
#define MAX_HISTORY 5
String eventHistory[MAX_HISTORY];
int historyIndex = 0;

// 状态变量
unsigned long lastEventTime = 0;

void updateDisplay(String title, String currentEvent, String info = "") {
    menuSprite.fillSprite(TFT_BLACK);
    
    // 显示标题
    menuSprite.setTextSize(2);
    menuSprite.drawString(title, TITLE_X, TITLE_Y);
    
    // 显示当前事件（较大字体）
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    menuSprite.drawString(currentEvent, EVENT_X, EVENT_Y);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // 显示附加信息
    if (info != "") {
        menuSprite.setTextSize(1);
        menuSprite.drawString(info, EVENT_X, EVENT_Y + 25);
        menuSprite.setTextSize(2);
    }
    
    // 显示历史记录
    menuSprite.setTextSize(1);
    menuSprite.drawString("Recent events:", HISTORY_X, HISTORY_Y);
    
    int yPos = HISTORY_Y + 15;
    for (int i = 0; i < MAX_HISTORY; i++) {
        int idx = (historyIndex + i) % MAX_HISTORY;
        if (eventHistory[idx] != "") {
            menuSprite.drawString(eventHistory[idx], HISTORY_X + 5, yPos);
            yPos += 12;
        }
    }
    
    // 显示时间
    menuSprite.drawString("Time: " + String(millis() / 1000) + "s", HISTORY_X, 200);
    
    menuSprite.pushSprite(0, 0);
    lastEventTime = millis();
}
void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    menuSprite.createSprite(239, 239);
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2);
    
    // 初始化历史记录
    for (int i = 0; i < MAX_HISTORY; i++) {
        eventHistory[i] = "";
    }
    
    initRotaryEncoder();
    
    // 显示初始界面
    updateDisplay("Rotary Encoder", "Ready", "Waiting for input...");
}

void addToHistory(String event) {
    eventHistory[historyIndex] = event;
    historyIndex = (historyIndex + 1) % MAX_HISTORY;
}



void checkDisplayTimeout() {
    // 如果超过显示持续时间，恢复就绪状态但保留历史
    if (millis() - lastEventTime > DISPLAY_DURATION) {
        updateDisplay("Rotary Encoder", "Ready", "Last: " + eventHistory[(historyIndex + MAX_HISTORY - 1) % MAX_HISTORY]);
    }
}

void loop() {
    // 检查显示超时
    checkDisplayTimeout();

    // 持续调用readButton()以更新状态机
    int clickEvent = readButton();

    // 检查各种事件
    int rotation = readEncoder();
    if (rotation != 0) {
        String direction = (rotation == 1) ? "CW" : "CCW";
        addToHistory("Rotated: " + direction);
        updateDisplay("Encoder Action", "Rotation", "Direction: " + direction);
    }

    if (clickEvent) {
        addToHistory("Single Click");
        updateDisplay("Button Action", "Click", "Single press");
    }

    if (readButtonLongPress()) {
        addToHistory("Long Press");
        updateDisplay("Button Action", "Long Press", "Hold detected");
    }

    if (readDoubleClick()) {
        addToHistory("Double Click");
        updateDisplay("Button Action", "Double Click", "Two quick presses");
    }

    delay(1); // 短暂延时
}
```

有关驱动TFT屏幕的内容可参考https://blog.csdn.net/xiaomiao2006/article/details/151123327

## 六、效果展示



## 七、代码解读

### 1. 核心状态变量

```cpp
// --- 旋转编码器状态变量 ---
static volatile int encoderPos = 0;
static uint8_t lastEncoded = 0;
static int deltaSum = 0;
static unsigned long lastEncoderTime = 0;

// --- 按键状态机变量 ---
// 状态定义
enum ButtonState {
  BTN_IDLE,           // 空闲状态：无按键动作
  BTN_PRESSED,        // 第一次按下：等待释放或转为长按
  BTN_RELEASED,       // 第一次按下后已释放：等待第二次按下（双击）或超时（单击）
  BTN_DOUBLE_PRESSED  // 第二次按下：等待释放以完成双击
};
static ButtonState buttonFSM = BTN_IDLE; // 按键状态机当前状态

// 时间戳与标志位
static unsigned long buttonPressStartTime = 0; // 当前按下状态的开始时间（用于长按检测）
static unsigned long firstReleaseTime = 0;     // 第一次释放的时间戳（用于双击超时判断）
static bool longPressHandled = false;          // 长按已处理标志
static bool clickHandled = false;              // 单击已处理标志（防止双击中的第一次单击被误报）
static bool doubleClickDetected = false;       // 双击已发生标志（由状态机设置，由readDoubleClick()读取并清除）

// --- 参数配置 ---
#define SWAP_CLK_DT 0
static const unsigned long longPressThreshold = 2000;     // 长按判定阈值(ms)
static const unsigned long progressBarStartTime = 1000;   // 进度条开始显示时间(ms)
static const unsigned long doubleClickTimeout = 300;      // 双击时间窗口(ms)，从第一次释放开始计算
```

### 2. 核心按键状态机 `readButton()`

它必须被频繁调用。

```cpp
int readButton() {
  int currentButtonState = digitalRead(ENCODER_SW);
  bool singleClickTriggered = false;
  unsigned long currentTime = millis();

  // 状态机逻辑
  switch (buttonFSM) {
    case BTN_IDLE:
      if (currentButtonState == LOW) { // 检测到下降沿，按键被按下
        buttonFSM = BTN_PRESSED;
        buttonPressStartTime = currentTime; // 记录按下时刻，用于长按计时
        longPressHandled = false;  // 重置长按处理标志
        clickHandled = false;      // 重置单击处理标志
        doubleClickDetected = false; // 重置双击标志
      }
      break;

    case BTN_PRESSED:
      if (currentButtonState == HIGH) { // 检测到上升沿，按键被释放
        firstReleaseTime = currentTime; // 记录第一次释放时刻
        buttonFSM = BTN_RELEASED;       // 转移至“等待双击”状态
      }
      // 注意：长按检测是在这个状态下，由另一个函数 readButtonLongPress() 处理的
      break;

    case BTN_RELEASED:
      // 检查双击超时：如果距离第一次释放的时间超过了双击窗口，则判定为一次单击
      if (currentTime - firstReleaseTime > doubleClickTimeout) {
        if (!clickHandled) { // 确保只触发一次
          singleClickTriggered = true; // 触发单击事件
          clickHandled = true;
        }
        buttonFSM = BTN_IDLE; // 回归空闲状态，等待下一次按键
      }
      // 在超时前，检查是否发生了第二次按下
      if (currentButtonState == LOW) {
        buttonFSM = BTN_DOUBLE_PRESSED;
        buttonPressStartTime = currentTime; // 重置按下时间，防止此次按下触发长按
      }
      break;

    case BTN_DOUBLE_PRESSED:
      if (currentButtonState == HIGH) { // 第二次按下后释放
        doubleClickDetected = true;     // 标记双击事件已发生
        buttonFSM = BTN_IDLE;           // 完成整个双击序列，回归空闲状态
      }
      break;
  }
  return singleClickTriggered; // 返回是否触发了单击
}
```

### 3. 长按检测函数 `readButtonLongPress()`

此函数需与状态机配合工作，它只在`BTN_PRESSED`状态下生效。

```cpp
bool readButtonLongPress() {
  bool triggered = false; // 本次调用是否触发了长按事件的标志

  // 仅在状态为“按下”(BTN_PRESSED) 且长按事件尚未被处理过的状态下进行检测
  if (buttonFSM == BTN_PRESSED && !longPressHandled) {
    unsigned long currentHoldTime = millis() - buttonPressStartTime; // 计算当前已按压的时间

    // 显示进度条逻辑：如果按压时间超过了进度条开始显示的时间阈值
    if (currentHoldTime >= progressBarStartTime) {
        // 计算进度比例 (0.0 到 1.0)
        // 从progressBarStartTime开始计算，到longPressThreshold达到100%
        float progress = (float)(currentHoldTime - progressBarStartTime) / (longPressThreshold - progressBarStartTime);
        progress = (progress > 1.0) ? 1.0 : progress; // 确保进度不超过1.0
        // 在menuSprite（菜单画布）上绘制进度条外框和填充
        menuSprite.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_WHITE); // 绘制白色外框
        menuSprite.fillRect(BAR_X + 2, BAR_Y + 2, (int)((BAR_WIDTH - 4) * progress), BAR_HEIGHT - 4, TFT_BLUE); // 用蓝色填充内部，宽度随进度增加
        menuSprite.pushSprite(0,0); // 将menuSprite的内容推送到屏幕上显示
    }

    // 长按触发判断：如果按压时间超过了长按阈值
    if (currentHoldTime >= longPressThreshold) {
        triggered = true;        // 标记触发了长按事件
        longPressHandled = true; // 设置长按已处理标志，防止重复触发
        clickHandled = true;     // 同时标记单击已处理，因为长按已经发生，后续的释放不应再触发单击事件
    }
  }

  // 进度条清理逻辑：当按键是释放状态（HIGH）
  if (digitalRead(ENCODER_SW) == HIGH) {
      // 并且之前有按下记录，且按下时间曾经超过显示进度条的阈值
      if (buttonPressStartTime != 0 && (millis() - buttonPressStartTime) > progressBarStartTime) {
          // 则用黑色填充清除进度条区域
          menuSprite.fillRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_BLACK);
          menuSprite.pushSprite(0,0); // 更新屏幕显示
      }
  }

  return triggered; // 返回是否检测到长按
}
```

### 4. 双击检测函数 `readDoubleClick()`

这个函数很简单，只是检查并清除一个标志位。

```cpp
bool readDoubleClick() {
  if (doubleClickDetected) {
    doubleClickDetected = false; // 读取后立即清除标志，保证只返回一次true
    return true;
  }
  return false;
```

## 八、改进方向

1. 当前是轮询方式。对于主循环任务繁重的系统，可以将CLK/DT引脚接入外部中断，在中断服务程序中快速处理状态变化，进一步提高响应速度。
2. 使用回调函数机制

谢谢大家