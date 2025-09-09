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

// 读取编码器旋转方向
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
      // 在超时检查之前或之后，检查是否发生了第二次按下
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

// 检测双击事件
// 返回值: true 如果检测到一个双击事件（仅返回一次），否则 false
bool readDoubleClick() {
  if (doubleClickDetected) { // 检查双击标志是否被状态机设置
    doubleClickDetected = false; // 读取后立即清除标志，确保事件只被报告一次
    return true;
  }
  return false;
}