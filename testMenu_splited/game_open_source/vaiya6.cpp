#include <TFT_eSPI.h>
#include "esp_system.h"  // 用于 esp_random()
#include "p_vaiiya.h"     // PROGMEM 图像数组
#include <Preferences.h>  // 用于掉电保存

TFT_eSPI tft = TFT_eSPI();

// 网格参数
const int GRID_ROWS = 7;
const int GRID_COLS = 7;

// IO15 按键
const int BUTTON_PIN = 15;
const unsigned long BUTTON_DEBOUNCE_MS = 40; // 去抖时间（毫秒）
bool buttonLastStablePressed = false; // 使用 INPUT_PULLUP，按下为低电平
bool buttonLastReadPressed = false;
unsigned long buttonLastChangeMs = 0;

// IO13 呼吸灯（LEDC PWM）
const int LED_PIN = 13;
const int LEDC_CHANNEL = 7;           // 选择一个未被占用的通道
const int LEDC_FREQ = 5000;           // PWM 频率（Hz）
const int LEDC_RESOLUTION_BITS = 10;  // 分辨率 10 位（0..1023）
const int LED_MAX_PERCENT = 10;       // 最大亮度百分比（可调），例如 35% 峰值
int ledDuty = 0;                      // 当前占空比
int ledStep = 1;                      // 每次变化步进（越大越快）
unsigned long ledLastUpdateMs = 0;
const unsigned long LED_UPDATE_INTERVAL_MS = 20; // 呼吸变化步进间隔

// 页面状态
enum PageId { PAGE_GRID = 1, PAGE_IMAGE = 2, PAGE_3 = 3, PAGE_GAME = 4 };
volatile PageId currentPage = PAGE_GRID;
volatile bool inTransition = false;
// 前置声明（用于在定义前被调用）
void revealGridLinesGradually(int perLineDelayMs = 10);
void hideGridLinesGradually(int perLineDelayMs = 10);
void enterSquareFromTopTo(int targetRow, int targetCol);
void fadeSquareToWhite(uint8_t steps = 14, uint16_t from = TFT_RED, uint16_t to = TFT_WHITE, int delayMs = 20);
// 长按进入游戏
const unsigned long LONG_PRESS_MS = 700; // 长按阈值（毫秒）
unsigned long buttonPressStartMs = 0;
bool buttonWaitingReleaseAction = false; // 用于短按在释放时执行页面切换



// 启动图片尺寸
#define IMG_W 188
#define IMG_H 50

// 计算得到的几何参数
int cellSize = 0;          // 单元格边长（像素）
int gridPixelW = 0;        // 网格总宽度（像素）
int gridPixelH = 0;        // 网格总高度（像素）
int originX = 0;           // 网格左上角 x
int originY = 0;           // 网格左上角 y

// 红色方块状态
int curRow = 0;
int curCol = 0;
unsigned long nextMoveAtMs = 0;

// 像素级精灵状态
int spriteSize = 0;     // 方块尺寸（与内框尺寸一致）
int posX = 0;           // 当前左上角 X（像素）
int posY = 0;           // 当前左上角 Y（像素）
const int stepPx = 2;   // 每步动画像素
// 页面3常量
const int PAGE3_LIFT = 10; // 图片上移像素
int page3ImageYOffset = 0; // 页面3图片相对中心Y偏移（向上为负）
int page3TextX = -1, page3TextY = -1, page3TextW = 0, page3TextH = 0; // 页面3文本区域
int page3RedStartX = -1, page3RedStartY = -1; // 替换前红色质心（屏幕坐标）


// --- Flappy 游戏状态 ---
// 黑色背景，白色水管，红色小鸟（比页面1的红块略小）
bool gameRunning = false;
int gameBirdSize = 0;        // 小鸟边长
int gameBirdX = 0;           // 小鸟左上角 X
int gameBirdY = 0;           // 小鸟左上角 Y
int gamePrevBirdX = 0;       // 上一帧位置（用于局部清除）
int gamePrevBirdY = 0;
float gameVelY = 0.0f;       // 垂直速度
const float GAME_GRAVITY = 0.18f; // 重力加速度（像素/帧^2）降速
const float GAME_FLAP_VY = -3.2f; // 拍打瞬时速度（向上）降速

// 水管参数（单列水管从右向左移动）
int pipeX = 0;               // 当前水管左侧 X
int pipePrevX = 0;           // 上一帧 X
int pipeGapY = 0;            // 缝隙顶部 Y
int pipeGapH = 0;            // 缝隙高度
int pipeW = 0;               // 水管宽度
int pipeSpeed = 0;           // 每帧像素速度（适度降低）
bool pipePassed = false;     // 是否已通过当前水管（用于计分）

// 计分系统
int currentScore = 0;        // 当前分数
int highScore = 0;           // 历史最高分
bool scoreChanged = false;   // 分数是否发生变化（用于重绘）

unsigned long gameLastStepMs = 0;  // 用于限帧
const unsigned long GAME_STEP_MS = 20; // 约 50 FPS 降速

// 掉电保存
Preferences preferences;

// 局部矩形黑色清除（已存在 fillBlackClipped，可直接使用）

// 绘制分数显示
void gameDrawScore() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  
  // 当前分数
  char scoreStr[16];
  sprintf(scoreStr, "Score: %d", currentScore);
  tft.setCursor(5, 5);
  tft.print(scoreStr);
  
  // 最高分
  char highStr[16];
  sprintf(highStr, "Best: %d", highScore);
  int highW = tft.textWidth(highStr);
  tft.setCursor(tft.width() - highW - 5, 5);
  tft.print(highStr);
}

// 清除分数区域
void gameEraseScore() {
  tft.fillRect(0, 0, tft.width(), 20, TFT_BLACK);
}

// 保存最高分到闪存
void saveHighScore() {
  preferences.putInt("highScore", highScore);
}

// 从闪存加载最高分
void loadHighScore() {
  highScore = preferences.getInt("highScore", 0);
}

void gameDrawPipeAt(int x) {
  int screenH = tft.height();
  // 上管，但保护分数区域
  if (pipeGapY > 20) {
    // 如果缝隙顶部在分数区域下方，正常绘制
    tft.fillRect(x, 0, pipeW, pipeGapY, TFT_WHITE);
  } else if (pipeGapY > 0) {
    // 如果缝隙顶部在分数区域内，只绘制分数区域下方的部分
    tft.fillRect(x, 20, pipeW, pipeGapY - 20, TFT_WHITE);
  }
  // 下管
  int bottomY = pipeGapY + pipeGapH;
  if (bottomY < screenH) {
    tft.fillRect(x, bottomY, pipeW, screenH - bottomY, TFT_WHITE);
  }
}

void gameErasePipeAt(int x) {
  int screenH = tft.height();
  // 黑色背景擦除
  fillBlackClipped(x, 0, pipeW, pipeGapY);
  int bottomY = pipeGapY + pipeGapH;
  if (bottomY < screenH) {
    fillBlackClipped(x, bottomY, pipeW, screenH - bottomY);
  }
}

inline void gameDrawBirdAt(int x, int y) {
  // 保护分数区域，如果小鸟在分数区域内，只绘制分数区域下方的部分
  if (y < 20) {
    // 小鸟顶部在分数区域内
    int overlapY = 20 - y;
    if (overlapY < gameBirdSize && overlapY > 0) {
      // 只绘制分数区域下方的部分
      tft.fillRect(x, 20, gameBirdSize, gameBirdSize - overlapY, TFT_RED);
    }
  } else {
    // 小鸟完全在分数区域下方，正常绘制
    tft.fillRect(x, y, gameBirdSize, gameBirdSize, TFT_RED);
  }
}

inline void gameEraseBirdAt(int x, int y) {
  // 黑色背景擦除
  fillBlackClipped(x, y, gameBirdSize, gameBirdSize);
}

void gameResetPipe(int screenW, int screenH) {
  pipeW = max(14, innerCellSize() / 2);
  pipeSpeed = max(1, innerCellSize() / 12); // 进一步降低水管速度
  pipeX = screenW + 4; // 从屏幕外开始
  pipePrevX = pipeX;
  // 缝隙高度与小鸟尺寸相关
  pipeGapH = max(gameBirdSize * 3, screenH / 3);
  pipeGapY = random(10, max(11, screenH - pipeGapH - 10));
  pipePassed = false; // 重置通过状态
}

void startGame() {
  int screenW = tft.width();
  int screenH = tft.height();
  // 清屏为黑
  tft.fillScreen(TFT_BLACK);

  // 小鸟尺寸略小于页面1红块
  gameBirdSize = max(8, innerCellSize() * 3 / 4);
  gameBirdX = screenW / 4 - gameBirdSize / 2;
  gameBirdY = screenH / 2 - gameBirdSize / 2;
  gamePrevBirdX = gameBirdX;
  gamePrevBirdY = gameBirdY;
  gameVelY = 0.0f;

  // 重置分数
  currentScore = 0;
  scoreChanged = true;

  gameResetPipe(screenW, screenH);

  // 初次绘制小鸟
  tft.startWrite();
  gameDrawBirdAt(gameBirdX, gameBirdY);
  tft.endWrite();
  
  // 等待一小段时间
  delay(200);

  // 1. 文字从屏幕外向下滑动至合适位置
  animateScoreTextEntry(screenW, screenH);
  
  // 2. 水管从屏幕外向左移动到预定位置
  animatePipeEntry(screenW, screenH);

  gameLastStepMs = millis();
  gameRunning = true;
  currentPage = PAGE_GAME;
}

// 使用当前屏幕上的红色方块作为小鸟起点，不清屏，避免闪屏
void startGameUsingExistingRedSquare() {
  int screenW = tft.width();
  int screenH = tft.height();
  gameBirdSize = spriteSize;
  gameBirdX = posX;
  gameBirdY = posY;
  gamePrevBirdX = gameBirdX;
  gamePrevBirdY = gameBirdY;
  gameVelY = 0.0f;
  
  // 重置分数
  currentScore = 0;
  scoreChanged = true;
  
  gameResetPipe(screenW, screenH);

  // 等待一小段时间让红色方块动画完全结束
  delay(200);

  // 1. 文字从屏幕外向下滑动至合适位置
  animateScoreTextEntry(screenW, screenH);
  
  // 2. 水管从屏幕外向左移动到预定位置
  animatePipeEntry(screenW, screenH);
  
  // 3. 开始游戏
  gameLastStepMs = millis();
  gameRunning = true;
  currentPage = PAGE_GAME;
}

// 在黑色背景下清除矩形
inline void clearRectBlack(int x, int y, int w, int h) {
  fillBlackClipped(x, y, w, h);
}

// 从当前大小线性缩小到目标大小（白色方块），局部重绘
void shrinkSquareToSize(int targetSize, int durationMs = 200, int steps = 10) {
  if (targetSize <= 0) return;
  int startSize = spriteSize;
  if (startSize == targetSize) return;
  
  for (int i = 1; i <= steps; ++i) {
    int newSize = startSize + (int)((long)(targetSize - startSize) * i / steps);
    
    tft.startWrite();
    // 先绘制新尺寸的方块
    int oldSize = spriteSize;
    spriteSize = newSize;
    drawSquareAt(posX, posY, TFT_WHITE);
    
    // 再清除拖尾区域（旧方块超出新方块的部分）
    if (oldSize > newSize) {
      // 缩小：清除右侧和下方的拖尾
      if (oldSize > newSize) {
        // 清除右侧拖尾
        tft.fillRect(posX + newSize, posY, oldSize - newSize, oldSize, TFT_BLACK);
        // 清除下方拖尾
        tft.fillRect(posX, posY + newSize, newSize, oldSize - newSize, TFT_BLACK);
      }
    } else if (oldSize < newSize) {
      // 放大：清除新方块区域内的旧内容（如果有的话）
      // 这里不需要额外清除，因为新方块已经覆盖了旧区域
    }
    tft.endWrite();
    
    delay(max(1, durationMs / steps));
  }
}

// 从页面1进入游戏的过渡动画（局部重绘）：
// 1) 网格逐渐隐藏 2) 红方块红->白 3) 方块线性缩小 4) 方块移动到游戏初始位置 5) 白->红 6) 启动游戏
void transitionPage1ToGame() {
  if (inTransition) return;
  inTransition = true;

  // 1) 网格逐渐隐藏
  hideGridLinesGradually();

  // 计算游戏起点与目标尺寸
  int screenW = tft.width();
  int screenH = tft.height();
  int targetSize = max(8, innerCellSize() * 3 / 4);
  int targetX = screenW / 4 - targetSize / 2;
  int targetY = screenH / 2 - targetSize / 2;

  // 2) 红->白
  fadeSquareToWhite();

  // 3) 线性缩小
  shrinkSquareToSize(targetSize);

  // 4) 移动到游戏起点（白色），使用黑底过渡步进
  while (posX != targetX) {
    int delta = targetX - posX;
    int step = (delta > 0) ? stepPx : -stepPx;
    if (abs(delta) < abs(step)) step = delta;
    slideStepToTransition(posX + step, posY, TFT_WHITE);
  }
  delay(120);
  while (posY != targetY) {
    int delta = targetY - posY;
    int step = (delta > 0) ? stepPx : -stepPx;
    if (abs(delta) < abs(step)) step = delta;
    slideStepToTransition(posX, posY + step, TFT_WHITE);
  }

  // 5) 白->红
  const uint8_t fadeSteps = 14;
  for (uint8_t i = 1; i <= fadeSteps; ++i) {
    uint16_t c = blend565(TFT_WHITE, TFT_RED, i, fadeSteps);
    tft.startWrite();
    drawSquareAt(posX, posY, c);
    tft.endWrite();
    delay(20);
  }

  // 6) 使用当前红色方块作为小鸟开始游戏
  startGameUsingExistingRedSquare();
  inTransition = false;
}


void animateMoveTo(int targetRow, int targetCol, uint16_t color = TFT_RED) {
  (void)color; // 运动时的颜色在函数内处理
  // 计算目标像素位置（内框左上角）
  int targetX = innerCellX(targetCol);
  int targetY = innerCellY(targetRow);

  // 移动前先将当前方块从红色渐变为白色
  fadeSquareToWhite();

  bool hasHoriz = (posX != targetX);
  bool hasVert = (posY != targetY);

  // 先做水平方向滑动
  while (posX != targetX) {
    int delta = targetX - posX;
    int step = (delta > 0) ? stepPx : -stepPx;
    if (abs(delta) < abs(step)) step = delta;
    slideStepTo(posX + step, posY, TFT_WHITE);
  }
  // 若有垂直段，拐弯处暂停
  if (hasHoriz && hasVert) {
    delay(200);//方块运动时转向等待时间
  }
  // 再做垂直方向滑动
  while (posY != targetY) {
    int delta = targetY - posY;
    int step = (delta > 0) ? stepPx : -stepPx;
    if (abs(delta) < abs(step)) step = delta;
    slideStepTo(posX, posY + step, TFT_WHITE);
  }

  // 更新离散单元格索引
  curRow = targetRow;
  curCol = targetCol;

  // 静止处从白色渐变为红色
  const uint8_t fadeSteps = 14; // 可调整以更平滑
  for (uint8_t i = 1; i <= fadeSteps; ++i) {
    uint16_t c = blend565(TFT_WHITE, TFT_RED, i, fadeSteps);
    tft.startWrite();
    drawSquareAt(posX, posY, c);
    tft.endWrite();
    delay(20);
  }
}

void endGameToGrid() {
  // 1. 先完全清除所有拖尾，确保画面干净
  int screenW = tft.width();
  int screenH = tft.height();
  
  // 清除小鸟拖尾
  int birdDx = gameBirdX - gamePrevBirdX;
  int birdDy = gameBirdY - gamePrevBirdY;
  
  tft.startWrite();
  if (birdDx != 0) {
    int trailX = birdDx > 0 ? gamePrevBirdX : gameBirdX + gameBirdSize;
    int trailY = gamePrevBirdY;
    int trailW = birdDx > 0 ? birdDx : -birdDx;
    int trailH = gameBirdSize;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  } else if (birdDy != 0) {
    int trailX = gamePrevBirdX;
    int trailY = birdDy > 0 ? gamePrevBirdY : gameBirdY + gameBirdSize;
    int trailW = gameBirdSize;
    int trailH = birdDy > 0 ? birdDy : -birdDy;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  }
  
  // 清除水管拖尾
  int pipeDx = pipeX - pipePrevX;
  if (pipeDx != 0) {
    int trailX = pipeDx > 0 ? pipePrevX + pipeW : pipeX + pipeW;
    int trailY = 0;
    int trailW = pipeDx > 0 ? pipeDx : -pipeDx;
    int trailH = screenH;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  }
  tft.endWrite();
  
  // 2. 现在可以安全地闪烁红色方块
  blinkGameBirdTwice();
  
  // 2. 画面中的其他元素（水管和分数）逐渐消失
  // 使用类似开机动画的抖动消失效果
  
  
  int phases = 16, lineDelayMs = 12;
  
  for (int p = 0; p < phases; ++p) {
    tft.startWrite();
    for (int y = p; y < screenH; y += phases) {
      // 擦除水管区域（如果水管还在屏幕上）
      if (pipeX + pipeW > 0 && y >= 0 && y < screenH) {
        // 擦除水管在这一行的部分
        if (y < pipeGapY) {
          // 上管区域
          if (pipeX < screenW) {
            int eraseX = max(0, pipeX);
            int eraseW = min(pipeW, screenW - eraseX);
            if (eraseW > 0) {
              tft.drawFastHLine(eraseX, y, eraseW, TFT_BLACK);
            }
          }
        } else if (y >= pipeGapY + pipeGapH) {
          // 下管区域
          if (pipeX < screenW) {
            int eraseX = max(0, pipeX);
            int eraseW = min(pipeW, screenW - eraseX);
            if (eraseW > 0) {
              tft.drawFastHLine(eraseX, y, eraseW, TFT_BLACK);
            }
          }
        }
      }
      
      // 擦除分数区域（顶部20像素）
      if (y < 20) {
        tft.drawFastHLine(0, y, screenW, TFT_BLACK);
      }
    }
    tft.endWrite();
    delay(lineDelayMs);
  }
  
  // 确保完全清除
  tft.startWrite();
  // 清除水管区域
  if (pipeX + pipeW > 0) {
    // 清除上管
    if (pipeGapY > 0) {
      tft.fillRect(max(0, pipeX), 0, min(pipeW, screenW - max(0, pipeX)), pipeGapY, TFT_BLACK);
    }
    // 清除下管
    int bottomY = pipeGapY + pipeGapH;
    if (bottomY < screenH) {
      tft.fillRect(max(0, pipeX), bottomY, min(pipeW, screenW - max(0, pipeX)), screenH - bottomY, TFT_BLACK);
    }
  }
  // 清除分数区域
  tft.fillRect(0, 0, screenW, 20, TFT_BLACK);
  tft.endWrite();
  

  
  // 4. 红色方块渐变为白色
  fadeGameBirdToWhite();
  
  // 5. 移动到屏幕中心（横平竖直运动）
  int centerX = screenW / 2 - gameBirdSize / 2;
  int centerY = screenH / 2 - gameBirdSize / 2;
  
  // 先水平移动，再垂直移动（与页面1相同）
  while (gameBirdX != centerX) {
    int dx = centerX - gameBirdX;
    int stepX = (dx > 0) ? 2 : -2;
    if (abs(dx) < abs(stepX)) stepX = dx;
    
    int newX = gameBirdX + stepX;
    
         // 使用拖尾擦除逻辑
     tft.startWrite();
     int trailX = stepX > 0 ? gameBirdX : newX + gameBirdSize;
     int trailY = gameBirdY;
     int trailW = stepX > 0 ? stepX : -stepX;
     int trailH = gameBirdSize;
     fillBlackClipped(trailX, trailY, trailW, trailH);
     tft.fillRect(newX, gameBirdY, gameBirdSize, gameBirdSize, TFT_WHITE);
     tft.endWrite();
    
    gameBirdX = newX;
    delay(8);
  }
  
  // 转向时暂停
  delay(200);
  
  // 再垂直移动
  while (gameBirdY != centerY) {
    int dy = centerY - gameBirdY;
    int stepY = (dy > 0) ? 2 : -2;
    if (abs(dy) < abs(stepY)) stepY = dy;
    
    int newY = gameBirdY + stepY;
    
    // 使用拖尾擦除逻辑
    tft.startWrite();
    int trailX = gameBirdX;
    int trailY = stepY > 0 ? gameBirdY : newY + gameBirdSize;
    int trailW = gameBirdSize;
    int trailH = stepY > 0 ? stepY : -stepY;
    fillBlackClipped(trailX, trailY, trailW, trailH);
    tft.fillRect(gameBirdX, newY, gameBirdSize, gameBirdSize, TFT_WHITE);
    tft.endWrite();
    
    gameBirdY = newY;
    delay(8);
  }
  
  // 6. 白色渐变为红色
  const uint8_t fadeSteps = 14;
  for (uint8_t i = 1; i <= fadeSteps; ++i) {
    uint16_t c = blend565(TFT_WHITE, TFT_RED, i, fadeSteps);
    tft.startWrite();
    tft.fillRect(gameBirdX, gameBirdY, gameBirdSize, gameBirdSize, c);
    tft.endWrite();
    delay(20);
  }

    // 3. 红色方块线性放大到与页面1中红色方块相同大小
  int targetSize = innerCellSize();
  int startSize = gameBirdSize;
  int startX = gameBirdX;
  int startY = gameBirdY;
  
  const int growSteps = 10;
  for (int i = 1; i <= growSteps; ++i) {
    int newSize = startSize + (int)((long)(targetSize - startSize) * i / growSteps);
    int newX = startX - (newSize - startSize) / 2; // 以中心为基准放大
    int newY = startY - (newSize - startSize) / 2;
    
    tft.startWrite();
    // 先绘制新方块
    tft.fillRect(newX, newY, newSize, newSize, TFT_RED);
    
    // 再清除拖尾区域（旧方块超出新方块的部分）
    if (startSize < newSize) {
      // 放大：清除新方块区域内的旧内容
      // 这里不需要额外清除，因为新方块已经覆盖了旧区域
    }
    tft.endWrite();
    
    // 更新位置和大小
    gameBirdX = newX;
    gameBirdY = newY;
    gameBirdSize = newSize;
    
    delay(50);
  }
  
  // 7. 白色网格逐渐出现
  computeGridGeometry();
  revealGridLinesGradually();
  
  // 8. 将方块移动到随机网格位置
  int targetRow = random(0, GRID_ROWS);
  int targetCol = random(0, GRID_COLS);
  
  // 更新方块状态为页面1的状态
  spriteSize = gameBirdSize;
  posX = gameBirdX;
  posY = gameBirdY;
  
  // 移动到目标网格位置
  animateMoveTo(targetRow, targetCol, TFT_RED);
  
  nextMoveAtMs = millis() + random(1000, 2001);
  currentPage = PAGE_GRID;
  gameRunning = false;
}

bool gameCollides() {
  // 与上下边界碰撞
  if (gameBirdY <= 0 || gameBirdY + gameBirdSize >= tft.height()) return true;
  // 与水管碰撞（轴对齐矩形）
  int birdRight = gameBirdX + gameBirdSize;
  int pipeRight = pipeX + pipeW;
  bool overlapX = !(birdRight <= pipeX || gameBirdX >= pipeRight);
  if (!overlapX) return false;
  int gapBottom = pipeGapY + pipeGapH;
  bool inGapY = (gameBirdY >= pipeGapY) && ((gameBirdY + gameBirdSize) <= gapBottom);
  return !inGapY;
}

void updateGame() {
  if (!gameRunning) return;
  unsigned long now = millis();
  if ((now - gameLastStepMs) < GAME_STEP_MS) return;
  gameLastStepMs = now;

  int screenW = tft.width();
  int screenH = tft.height();

  // 记录上一帧位置用于局部清除
  gamePrevBirdX = gameBirdX;
  gamePrevBirdY = gameBirdY;
  pipePrevX = pipeX;

  // 物理更新
  gameVelY += GAME_GRAVITY;
  gameBirdY += (int)gameVelY;
  pipeX -= pipeSpeed;

  // 计分检测：小鸟通过水管
  if (!pipePassed && gameBirdX > pipeX + pipeW) {
    currentScore++;
    pipePassed = true;
    scoreChanged = true;
    // 更新最高分
    if (currentScore > highScore) {
      highScore = currentScore;
      saveHighScore(); // 保存最高分
    }
  }

  // 水管循环
  if (pipeX + pipeW < 0) {
    gameResetPipe(screenW, screenH);
  }

  // 碰撞检测
  if (gameCollides()) {
    endGameToGrid();
    return;
  }

  // 局部重绘：严格按照页面1的无闪烁逻辑
  tft.startWrite();
  
  // 1. 擦除小鸟拖尾区域（只擦除移动产生的拖尾）
  int birdDx = gameBirdX - gamePrevBirdX;
  int birdDy = gameBirdY - gamePrevBirdY;
  
  if (birdDx != 0) {
    // 水平移动：擦除拖尾区域
    int trailX = birdDx > 0 ? gamePrevBirdX : gameBirdX + gameBirdSize;
    int trailY = gamePrevBirdY;
    int trailW = birdDx > 0 ? birdDx : -birdDx;
    int trailH = gameBirdSize;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  } else if (birdDy != 0) {
    // 垂直移动：擦除拖尾区域
    int trailX = gamePrevBirdX;
    int trailY = birdDy > 0 ? gamePrevBirdY : gameBirdY + gameBirdSize;
    int trailW = gameBirdSize;
    int trailH = birdDy > 0 ? birdDy : -birdDy;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  }
  
  // 2. 擦除水管拖尾区域（只擦除移动产生的拖尾）
  int pipeDx = pipeX - pipePrevX;
  if (pipeDx != 0) {
    // 水管从右向左移动，拖尾在右侧
    int trailX = pipeDx > 0 ? pipePrevX + pipeW : pipeX + pipeW;
    int trailY = 0;
    int trailW = pipeDx > 0 ? pipeDx : -pipeDx;
    int trailH = screenH;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  }
  
  // 3. 绘制新水管位置
  gameDrawPipeAt(pipeX);
  
  // 4. 绘制新小鸟位置
  gameDrawBirdAt(gameBirdX, gameBirdY);
  
  tft.endWrite();
  
  // 5. 每次更新后都重新绘制分数，确保分数始终在最上层
  gameDrawScore();
}

// 游戏中拍打（立即响应按下）
void gameFlap() {
  if (!gameRunning) return;
  gameVelY = GAME_FLAP_VY;
}





int innerCellX(int col) {
  return originX + col * cellSize + 1;
}

int innerCellY(int row) {
  return originY + row * cellSize + 1;
}

int innerCellSize() {
  return cellSize - 2;  // 四周各留 1px 以保留网格线
}

void computeGridGeometry() {
  int screenW = tft.width();
  int screenH = tft.height();

  cellSize = min(screenW / GRID_COLS, screenH / GRID_ROWS);
  gridPixelW = cellSize * GRID_COLS;
  gridPixelH = cellSize * GRID_ROWS;

  // 居中网格
  originX = (screenW - gridPixelW) / 2;
  originY = (screenH - gridPixelH) / 2;
}

void drawGrid() {
  tft.fillScreen(TFT_BLACK);
  tft.startWrite();
  // 竖直网格线
  for (int c = 0; c <= GRID_COLS; c++) {
    int x = originX + c * cellSize;
    tft.drawFastVLine(x, originY, gridPixelH, TFT_WHITE);
  }
  // 水平网格线
  for (int r = 0; r <= GRID_ROWS; r++) {
    int y = originY + r * cellSize;
    tft.drawFastHLine(originX, y, gridPixelW, TFT_WHITE);
  }
  tft.endWrite();
}

void fillCellInterior(int row, int col, uint16_t color) {
  tft.fillRect(innerCellX(col), innerCellY(row), innerCellSize(), innerCellSize(), color);
}

// 安全裁剪后的黑色填充，避免越界绘制
void fillBlackClipped(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  int rx = x < 0 ? 0 : x;
  int ry = y < 0 ? 0 : y;
  int rx2 = x + w; if (rx2 > tft.width()) rx2 = tft.width();
  int ry2 = y + h; if (ry2 > tft.height()) ry2 = tft.height();
  if (rx2 > rx && ry2 > ry) {
    tft.fillRect(rx, ry, rx2 - rx, ry2 - ry, TFT_BLACK);
  }
}

// 恢复矩形区域内经过的网格线（白色）
void restoreGridInRect(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  // Clip to grid bounds
  int rx = x < originX ? originX : x;
  int ry = y < originY ? originY : y;
  int rx2 = x + w; if (rx2 > originX + gridPixelW) rx2 = originX + gridPixelW;
  int ry2 = y + h; if (ry2 > originY + gridPixelH) ry2 = originY + gridPixelH;
  if (rx2 <= rx || ry2 <= ry) return;

  // 重绘矩形内的竖直网格线
  for (int c = 0; c <= GRID_COLS; c++) {
    int lineX = originX + c * cellSize;
    if (lineX >= rx && lineX < rx2) {
      tft.drawFastVLine(lineX, ry, ry2 - ry, TFT_WHITE);
    }
  }
  // 重绘矩形内的水平网格线
  for (int r = 0; r <= GRID_ROWS; r++) {
    int lineY = originY + r * cellSize;
    if (lineY >= ry && lineY < ry2) {
      tft.drawFastHLine(rx, lineY, rx2 - rx, TFT_WHITE);
    }
  }
}

inline void drawSquareAt(int x, int y, uint16_t color) {
  tft.fillRect(x, y, spriteSize, spriteSize, color);
}

// 执行一次小幅滑动至 (newX, newY)，并局部重绘
void slideStepTo(int newX, int newY, uint16_t color) {
  int dx = newX - posX;
  int dy = newY - posY;

  tft.startWrite();
  // 清除旧位置留下的拖尾区域
  if (dx != 0) {
    int trailX = dx > 0 ? posX : newX + spriteSize;
    int trailY = posY;
    int trailW = dx > 0 ? dx : -dx;
    int trailH = spriteSize;
    fillBlackClipped(trailX, trailY, trailW, trailH);
    restoreGridInRect(trailX, trailY, trailW, trailH);
  } else if (dy != 0) {
    int trailX = posX;
    int trailY = dy > 0 ? posY : newY + spriteSize;
    int trailW = spriteSize;
    int trailH = dy > 0 ? dy : -dy;
    fillBlackClipped(trailX, trailY, trailW, trailH);
    restoreGridInRect(trailX, trailY, trailW, trailH);
  }

  // 在新位置绘制
  drawSquareAt(newX, newY, color);
  tft.endWrite();

  posX = newX;
  posY = newY;
  delay(8);
}

// 混合两个 RGB565 颜色（t ∈ [0, tMax]）
uint16_t blend565(uint16_t c0, uint16_t c1, uint8_t t, uint8_t tMax) {
  if (t >= tMax) return c1;
  uint32_t r0 = (c0 >> 11) & 0x1F;
  uint32_t g0 = (c0 >> 5) & 0x3F;
  uint32_t b0 = c0 & 0x1F;
  uint32_t r1 = (c1 >> 11) & 0x1F;
  uint32_t g1 = (c1 >> 5) & 0x3F;
  uint32_t b1 = c1 & 0x1F;
  uint32_t r = (r0 * (tMax - t) + r1 * t + (tMax >> 1)) / tMax;
  uint32_t g = (g0 * (tMax - t) + g1 * t + (tMax >> 1)) / tMax;
  uint32_t b = (b0 * (tMax - t) + b1 * t + (tMax >> 1)) / tMax;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// 静止状态下将当前方块从红色渐变为白色
void fadeSquareToWhite(uint8_t steps, uint16_t from, uint16_t to, int delayMs) {
  for (uint8_t i = 1; i <= steps; ++i) {
    uint16_t c = blend565(from, to, i, steps);
    tft.startWrite();
    drawSquareAt(posX, posY, c);
    tft.endWrite();
    delay(delayMs);
  }
}




// --- 启动动画相关 ---

void showBootImage1s() {
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true); // 匹配 ImageConverter 565 输出
  int screenW = tft.width();
  int screenH = tft.height();
  int x = (screenW - IMG_W) / 2;
  int y = (screenH - IMG_H) / 2;
  tft.startWrite();
  tft.pushImage(x, y, IMG_W, IMG_H, (const uint16_t*)vaiiya);
  tft.endWrite();
  delay(1500);// 开机动画图片停留时长
}

void fadeToBlackDither(int phases = 16, int lineDelayMs = 12) {// 开机动画渐黑速度
  int screenW = tft.width();
  int screenH = tft.height();
  for (int p = 0; p < phases; ++p) {
    tft.startWrite();
    for (int y = p; y < screenH; y += phases) {
      tft.drawFastHLine(0, y, screenW, TFT_BLACK);
    }
    tft.endWrite();
    delay(lineDelayMs);
  }
  // 确保全黑
  tft.fillScreen(TFT_BLACK);
}

void blinkRedSquareTwice() {
  // 假设 posX/posY/spriteSize 已经设置
  for (int i = 0; i < 2; ++i) {
    tft.startWrite();
    drawSquareAt(posX, posY, TFT_BLACK);
    tft.endWrite();
    delay(300);// 开机动画红色闪烁间隔，下同
    tft.startWrite();
    drawSquareAt(posX, posY, TFT_RED);
    tft.endWrite();
    delay(300);
  }
}

// 游戏中小鸟闪烁两次
void blinkGameBirdTwice() {
  for (int i = 0; i < 2; ++i) {
    tft.startWrite();
    tft.fillRect(gameBirdX, gameBirdY, gameBirdSize, gameBirdSize, TFT_BLACK);
    tft.endWrite();
    delay(300);
    tft.startWrite();
    tft.fillRect(gameBirdX, gameBirdY, gameBirdSize, gameBirdSize, TFT_RED);
    tft.endWrite();
    delay(300);
  }
}

// 游戏中小鸟渐变为白色
void fadeGameBirdToWhite() {
  const uint8_t steps = 14;
  for (uint8_t i = 1; i <= steps; ++i) {
    uint16_t c = blend565(TFT_RED, TFT_WHITE, i, steps);
    tft.startWrite();
    tft.fillRect(gameBirdX, gameBirdY, gameBirdSize, gameBirdSize, c);
    tft.endWrite();
    delay(20);
  }
}

// 分数文字从屏幕外向下滑动进入动画
void animateScoreTextEntry(int screenW, int screenH) {
  // 计算分数文字位置
  const char *scoreStr = "Score: 0";
  const char *highStr = "Best: 0";
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  
  int scoreW = tft.textWidth(scoreStr);
  int highW = tft.textWidth(highStr);
  
  int scoreX = 5;
  int scoreY = 5;
  int highX = screenW - highW - 5;
  int highY = 5;
  
  // 文字从屏幕上方外部开始
  int startY = -20; // 文字高度约为16像素，从-20开始
  
  // 逐帧向下滑动
  for (int y = startY; y <= scoreY; y += 3) {
    tft.startWrite();
    
    // 清除上一帧的文字区域（避免拖尾）
    if (y > startY) {
      tft.fillRect(scoreX, y - 3, scoreW, 20, TFT_BLACK);
      tft.fillRect(highX, y - 3, highW, 20, TFT_BLACK);
    }
    
    // 绘制当前帧的文字
    tft.setCursor(scoreX, y);
    tft.print(scoreStr);
    tft.setCursor(highX, y);
    tft.print(highStr);
    
    tft.endWrite();
    delay(15); // 控制滑动速度
  }
  
  // 确保文字在最终位置完全显示
  tft.startWrite();
  tft.fillRect(scoreX, scoreY, scoreW, 20, TFT_BLACK);
  tft.fillRect(highX, highY, highW, 20, TFT_BLACK);
  tft.setCursor(scoreX, scoreY);
  tft.print(scoreStr);
  tft.setCursor(highX, highY);
  tft.print(highStr);
  tft.endWrite();
}

// 水管从屏幕外向左移动进入动画
void animatePipeEntry(int screenW, int screenH) {
  // 水管初始位置在屏幕右侧外部
  int startX = screenW + 10;
  int targetX = pipeX; // 目标位置
  
  // 逐帧向左移动
  for (int x = startX; x >= targetX; x -= 4) {
    tft.startWrite();
    
    // 清除上一帧的水管位置（避免拖尾）
    if (x < startX) {
      int prevX = x + 4;
      if (prevX < screenW) {
        // 清除上管拖尾
        if (pipeGapY > 20) {
          tft.fillRect(prevX, 0, 4, pipeGapY, TFT_BLACK);
        } else if (pipeGapY > 0) {
          tft.fillRect(prevX, 20, 4, pipeGapY - 20, TFT_BLACK);
        }
        // 清除下管拖尾
        int bottomY = pipeGapY + pipeGapH;
        if (bottomY < screenH) {
          tft.fillRect(prevX, bottomY, 4, screenH - bottomY, TFT_BLACK);
        }
      }
    }
    
    // 绘制当前帧的水管
    if (x < screenW) {
      // 上管
      if (pipeGapY > 20) {
        tft.fillRect(x, 0, pipeW, pipeGapY, TFT_WHITE);
      } else if (pipeGapY > 0) {
        tft.fillRect(x, 20, pipeW, pipeGapY - 20, TFT_WHITE);
      }
      // 下管
      int bottomY = pipeGapY + pipeGapH;
      if (bottomY < screenH) {
        tft.fillRect(x, bottomY, pipeW, screenH - bottomY, TFT_WHITE);
      }
    }
    
    tft.endWrite();
    delay(20); // 控制移动速度
  }
  
  // 确保水管在最终位置完全显示
  tft.startWrite();
  gameDrawPipeAt(pipeX);
  tft.endWrite();
}

// 逐渐显示网格线：竖向线条从上方移动下来，横向线条从左侧移动过来
void revealGridLinesGradually(int perLineDelayMs) {//网格动画速度
  // 逐条绘制竖直线：从上方移动下来
  for (int c = 0; c <= GRID_COLS; ++c) {
    int x = originX + c * cellSize;
    int lineLength = gridPixelH;
    
    // 竖直线从上方移动下来
    for (int step = 0; step <= lineLength; step += 3) { // 4像素步长，可调整
      tft.startWrite();
      // 绘制当前移动中的线段
      if (step > 0) {
        tft.drawFastVLine(x, originY, step, TFT_WHITE);
      }
      // 清除上方多余的线段（如果有的话）
      if (step < lineLength) {
        tft.drawFastVLine(x, originY + step, lineLength - step, TFT_BLACK);
      }
      tft.endWrite();
      delay(perLineDelayMs / 10); // 更快的动画速度
    }
    // 确保完全绘制
    tft.startWrite();
    tft.drawFastVLine(x, originY, lineLength, TFT_WHITE);
    tft.endWrite();
  }
  
  // 再逐条绘制水平线：从左侧移动过来
  for (int r = 0; r <= GRID_ROWS; ++r) {
    int y = originY + r * cellSize;
    int lineLength = gridPixelW;
    
    // 水平线从左侧移动过来
    for (int step = 0; step <= lineLength; step += 3) { // 4像素步长，可调整
      tft.startWrite();
      // 绘制当前移动中的线段
      if (step > 0) {
        tft.drawFastHLine(originX, y, step, TFT_WHITE);
      }
      // 清除右侧多余的线段（如果有的话）
      if (step < lineLength) {
        tft.drawFastHLine(originX + step, y, lineLength - step, TFT_BLACK);
      }
      tft.endWrite();
      delay(perLineDelayMs / 10); // 更快的动画速度
    }
    // 确保完全绘制
    tft.startWrite();
    tft.drawFastHLine(originX, y, lineLength, TFT_WHITE);
    tft.endWrite();
  }
}

void runBootAnimation() {
  showBootImage1s();
  fadeToBlackDither();

  // Place red square at a random cell on black background
  curRow = random(0, GRID_ROWS);
  curCol = random(0, GRID_COLS);
  spriteSize = innerCellSize();
  posX = innerCellX(curCol);
  posY = innerCellY(curRow);
  tft.startWrite();
  drawSquareAt(posX, posY, TFT_RED);
  tft.endWrite();

  blinkRedSquareTwice();
  revealGridLinesGradually();
}

// --- 页面切换相关 ---

// 逐渐隐藏网格线（与 revealGridLinesGradually 相反）
void hideGridLinesGradually(int perLineDelayMs) {//网格动画速度
  // 逐条擦除水平线：从左向右移动消失
  for (int r = GRID_ROWS; r >= 0; --r) {
    int y = originY + r * cellSize;
    int lineLength = gridPixelW;
    
    // 水平线从左向右移动消失
    for (int step = 0; step <= lineLength; step += 3) { // 4像素步长，可调整
      tft.startWrite();
      // 清除当前位置的线段
      if (step > 0) {
        tft.drawFastHLine(originX, y, step, TFT_BLACK);
      }
      // 绘制移动中的线段（从右侧开始缩短）
      if (step < lineLength) {
        tft.drawFastHLine(originX + step, y, lineLength - step, TFT_WHITE);
      }
      tft.endWrite();
      delay(perLineDelayMs / 10); // 更快的动画速度
    }
    // 确保完全清除
    tft.startWrite();
    tft.drawFastHLine(originX, y, lineLength, TFT_BLACK);
    tft.endWrite();
  }
  
  // 再逐条擦除竖直线：从下向上移动消失
  for (int c = GRID_COLS; c >= 0; --c) {
    int x = originX + c * cellSize;
    int lineLength = gridPixelH;
    
    // 竖直线从下向上移动消失
    for (int step = 0; step <= lineLength; step += 3) { // 4像素步长，可调整
      tft.startWrite();
      // 清除当前位置的线段
      if (step > 0) {
        tft.drawFastVLine(x, originY + lineLength - step, step, TFT_BLACK);
      }
      // 绘制移动中的线段（从上方开始缩短）
      if (step < lineLength) {
        tft.drawFastVLine(x, originY, lineLength - step, TFT_WHITE);
      }
      tft.endWrite();
      delay(perLineDelayMs / 10); // 更快的动画速度
    }
    // 确保完全清除
    tft.startWrite();
    tft.drawFastVLine(x, originY, lineLength, TFT_BLACK);
    tft.endWrite();
  }
}

// 按抖动分组的扫描线逐步显示图片（与 fadeToBlackDither 相反）
void revealImageDither(int phases = 16, int lineDelayMs = 12) {
  tft.setSwapBytes(true);
  int screenW = tft.width();
  int screenH = tft.height();
  int x = (screenW - IMG_W) / 2;
  int y0 = (screenH - IMG_H) / 2;
  for (int p = 0; p < phases; ++p) {
    tft.startWrite();
    for (int y = p; y < screenH; y += phases) {
      if (y >= y0 && y < y0 + IMG_H) {
        int srcRow = y - y0;
        const uint16_t* rowPtr = ((const uint16_t*)vaiiya) + srcRow * IMG_W;
        tft.pushImage(x, y, IMG_W, 1, rowPtr);
      } else {
        tft.drawFastHLine(0, y, screenW, TFT_BLACK);
      }
    }
    tft.endWrite();
    delay(lineDelayMs);
  }
}

// 将当前（白色）方块向上滑出屏幕
void slideSquareOutUp() {
  while (posY > -spriteSize) {
    int step = -stepPx;
    if (posY + step < -spriteSize) step = -spriteSize - posY;
    slideStepTo(posX, posY + step, TFT_WHITE);
  }
}

// 从屏幕上方外部降下白色方块至目标格后再渐变为红色
void enterSquareFromTopTo(int targetRow, int targetCol) {
  spriteSize = innerCellSize();
  int targetX = innerCellX(targetCol);
  int targetY = innerCellY(targetRow);
  posX = targetX;
  posY = -spriteSize;
  tft.startWrite();
  drawSquareAt(posX, posY, TFT_WHITE);
  tft.endWrite();
  while (posY != targetY) {
    int delta = targetY - posY;
    int step = (delta > 0) ? stepPx : -stepPx;
    if (abs(delta) < abs(step)) step = delta;
    slideStepTo(posX, posY + step, TFT_WHITE);
  }
  // 更新离散单元格索引
  curRow = targetRow;
  curCol = targetCol;
  // 静止处渐变为红色
  const uint8_t fadeSteps = 14;
  for (uint8_t i = 1; i <= fadeSteps; ++i) {
    uint16_t c = blend565(TFT_WHITE, TFT_RED, i, fadeSteps);
    tft.startWrite();
    drawSquareAt(posX, posY, c);
    tft.endWrite();
    delay(20);
  }
}

void transitionToPage2() {
  inTransition = true;
  // 流程：红→白、上滑消失、网格隐藏、图片显示
  fadeSquareToWhite();
  slideSquareOutUp();
  hideGridLinesGradually();
  revealImageDither();
  currentPage = PAGE_IMAGE;
  inTransition = false;
}

void transitionToPage1() {
  inTransition = true;
  // 图片以启动同款方式淡出到黑
  fadeToBlackDither();
  // 网格重新出现
  revealGridLinesGradually();
  // 方块自顶端进入随机格，随后恢复运动
  int targetRow = random(0, GRID_ROWS);
  int targetCol = random(0, GRID_COLS);
  enterSquareFromTopTo(targetRow, targetCol);
  nextMoveAtMs = millis() + random(1000, 2001);
  currentPage = PAGE_GRID;
  inTransition = false;
}

// --- 页面3相关与过渡 ---

// 判断是否为“偏红”的像素（RGB565）
inline bool isRedish(uint16_t c) {
  uint8_t r = (c >> 11) & 0x1F;
  uint8_t g = (c >> 5) & 0x3F;
  uint8_t b = c & 0x1F;
  return (r >= 24) && (g <= 8) && (b <= 8);
}

// 在启动图片中寻找红色色块的质心（屏幕坐标）
void findRedFeatureCenterOnImage(int &screenX, int &screenY) {
  int screenW = tft.width();
  int screenH = tft.height();
  int imgX = (screenW - IMG_W) / 2;
  int imgY = (screenH - IMG_H) / 2 + page3ImageYOffset;

  long sumX = 0, sumY = 0, cnt = 0;
  const uint16_t *img = (const uint16_t*)vaiiya;
  for (int y = 0; y < IMG_H; ++y) {
    const uint16_t *row = img + y * IMG_W;
    for (int x = 0; x < IMG_W; ++x) {
      uint16_t c = row[x];
      if (isRedish(c)) {
        sumX += x; sumY += y; cnt++;
      }
    }
  }
  if (cnt == 0) {
    // 回退为图片中心
    screenX = imgX + IMG_W / 2;
    screenY = imgY + IMG_H / 2;
  } else {
    int cx = (int)(sumX / cnt);
    int cy = (int)(sumY / cnt);
    screenX = imgX + cx;
    screenY = imgY + cy;
  }
}

// 将图片中的“红色像素”替换为白色（其余颜色保持不变）
void replaceImageRedWithWhite() {
  int screenW = tft.width();
  int screenH = tft.height();
  int x0 = (screenW - IMG_W) / 2;
  int y0 = (screenH - IMG_H) / 2 + page3ImageYOffset;
  const uint16_t *img = (const uint16_t*)vaiiya;
  tft.startWrite();
  for (int y = 0; y < IMG_H; ++y) {
    const uint16_t *row = img + y * IMG_W;
    int runStart = -1;
    for (int x = 0; x < IMG_W; ++x) {
      if (isRedish(row[x])) {
        if (runStart < 0) runStart = x;
      } else if (runStart >= 0) {
        tft.drawFastHLine(x0 + runStart, y0 + y, x - runStart, TFT_WHITE);
        runStart = -1;
      }
    }
    if (runStart >= 0) {
      tft.drawFastHLine(x0 + runStart, y0 + y, IMG_W - runStart, TFT_WHITE);
    }
  }
  tft.endWrite();
}

// 获取当前屏幕上图片的矩形
void getImageRect(int &x0, int &y0, int &w, int &h) {
  int screenW = tft.width();
  int screenH = tft.height();
  x0 = (screenW - IMG_W) / 2;
  y0 = (screenH - IMG_H) / 2 + page3ImageYOffset;
  w = IMG_W; h = IMG_H;
}

// 在任意位置绘制一张图片：将原图红色像素替换为白色，其余保持原色
void drawImageRedAsWhiteAt(int x0, int y0) {
  tft.setSwapBytes(true);
  static uint16_t lineBuf[IMG_W];
  const uint16_t *img = (const uint16_t*)vaiiya;
  tft.startWrite();
  for (int y = 0; y < IMG_H; ++y) {
    const uint16_t *row = img + y * IMG_W;
    for (int x = 0; x < IMG_W; ++x) {
      uint16_t c = row[x];
      lineBuf[x] = isRedish(c) ? TFT_WHITE : c;
    }
    tft.pushImage(x0, y0 + y, IMG_W, 1, lineBuf);
  }
  tft.endWrite();
}

// 在任意位置绘制原图（不做红色替换）
void drawOriginalImageAt(int x0, int y0) {
  tft.setSwapBytes(true);
  const uint16_t *img = (const uint16_t*)vaiiya;
  tft.startWrite();
  for (int y = 0; y < IMG_H; ++y) {
    const uint16_t *row = img + y * IMG_W;
    tft.pushImage(x0, y0 + y, IMG_W, 1, row);
  }
  tft.endWrite();
}

// 页面3专用步进：清拖尾时，图片区域内用白色，其他区域用黑色
void slideStepToPage3(int newX, int newY, uint16_t color) {
  int dx = newX - posX;
  int dy = newY - posY;

  int ix, iy, iw, ih;
  getImageRect(ix, iy, iw, ih);

  tft.startWrite();
  // 清除旧位置留下的拖尾区域
  if (dx != 0 || dy != 0) {
    int trailX = (dx != 0) ? (dx > 0 ? posX : newX + spriteSize) : posX;
    int trailY = (dx != 0) ? posY : (dy > 0 ? posY : newY + spriteSize);
    int trailW = (dx != 0) ? (dx > 0 ? dx : -dx) : spriteSize;
    int trailH = (dx != 0) ? spriteSize : (dy > 0 ? dy : -dy);

    // 交集部分填白
    int rx = max(trailX, ix);
    int ry = max(trailY, iy);
    int rx2 = min(trailX + trailW, ix + iw);
    int ry2 = min(trailY + trailH, iy + ih);
    if (rx2 > rx && ry2 > ry) {
      tft.fillRect(rx, ry, rx2 - rx, ry2 - ry, TFT_WHITE);
    }
    // 非交集部分填黑
    // 左侧
    if (trailX < rx) {
      tft.fillRect(trailX, trailY, rx - trailX, trailH, TFT_BLACK);
    }
    // 右侧
    if (rx2 < trailX + trailW) {
      tft.fillRect(rx2, trailY, (trailX + trailW) - rx2, trailH, TFT_BLACK);
    }
    // 上方
    int ux = max(trailX, rx);
    int uw = min(trailX + trailW, rx2) - ux;
    if (trailY < ry && uw > 0) {
      tft.fillRect(ux, trailY, uw, ry - trailY, TFT_BLACK);
    }
    // 下方
    int dx2 = max(trailX, rx);
    int dw = min(trailX + trailW, rx2) - dx2;
    if (ry2 < trailY + trailH && dw > 0) {
      tft.fillRect(dx2, ry2, dw, (trailY + trailH) - ry2, TFT_BLACK);
    }
  }

  // 在新位置绘制
  drawSquareAt(newX, newY, color);
  tft.endWrite();

  posX = newX;
  posY = newY;
  delay(8);
}

// 页面3：清除矩形，图片区域内用白，其它用黑
void clearRectPage3(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  int ix, iy, iw, ih;
  getImageRect(ix, iy, iw, ih);
  // 交集部分填白
  int rx = max(x, ix);
  int ry = max(y, iy);
  int rx2 = min(x + w, ix + iw);
  int ry2 = min(y + h, iy + ih);
  tft.startWrite();
  if (rx2 > rx && ry2 > ry) {
    tft.fillRect(rx, ry, rx2 - rx, ry2 - ry, TFT_WHITE);
  }
  // 左侧黑
  if (x < rx) tft.fillRect(x, y, rx - x, h, TFT_BLACK);
  // 右侧黑
  if (rx2 < x + w) tft.fillRect(rx2, y, (x + w) - rx2, h, TFT_BLACK);
  // 上方黑
  int ux = max(x, rx);
  int uw = min(x + w, rx2) - ux;
  if (y < ry && uw > 0) tft.fillRect(ux, y, uw, ry - y, TFT_BLACK);
  // 下方黑
  int dx2 = max(x, rx);
  int dw = min(x + w, rx2) - dx2;
  if (ry2 < y + h && dw > 0) tft.fillRect(dx2, ry2, dw, (y + h) - ry2, TFT_BLACK);
  tft.endWrite();
}

// 小红方块从图片中红色特征点出发，上移到屏幕顶，再向左至屏幕左上角；
// 在水平移动过程中由小逐渐变大到 spriteSize；到达左上角后闪烁两次
void animateImageToCornerRedSquare() {
  int startX = page3RedStartX;
  int startY = page3RedStartY;
  if (startX < 0 || startY < 0) {
    findRedFeatureCenterOnImage(startX, startY);
  }

  // 初始小方块尺寸
  int smallSize = max(10, innerCellSize() / 6);
  spriteSize = smallSize;
  posX = startX - smallSize / 2;
  posY = startY - smallSize / 2;
  tft.startWrite();
  drawSquareAt(posX, posY, TFT_RED);
  tft.endWrite();

  // 先向上移动到网格左上角内框的 Y（不改变大小）
  int stopY = innerCellY(0);
  while (posY > stopY) {
    int step = -stepPx;
    if (posY + step < stopY) step = stopY - posY;
    slideStepToPage3(posX, posY + step, TFT_RED);
  }
  // 转向时进行非瞬时放大：在约0.1s内线性插值变大
  int targetSize = innerCellSize();
  int oldSize = spriteSize;
  const int growSteps = 8; // ~8帧
  for (int i = 1; i <= growSteps; ++i) {
    int newSize = oldSize + (int)((long)(targetSize - oldSize) * i / growSteps);
    
    tft.startWrite();
    // 先绘制新尺寸的方块
    int currentSize = spriteSize;
    spriteSize = newSize;
    drawSquareAt(posX, posY, TFT_RED);
    
    // 再清除拖尾区域（旧方块超出新方块的部分）
    if (currentSize < newSize) {
      // 放大：清除新方块区域内的旧内容
      // 这里不需要额外清除，因为新方块已经覆盖了旧区域
    }
    tft.endWrite();
    
    delay(100 / growSteps);
  }
  // 再向左移动到网格左上角内框的 X（保持大小）
  int stopX = innerCellX(0);
  while (posX > stopX) {
    int step = -stepPx;
    if (posX + step < stopX) step = stopX - posX;
    slideStepToPage3(posX + step, posY, TFT_RED);
  }

  // 到达左上角后闪烁两次
  blinkRedSquareTwice();
}

// 图片略微上移，然后在其正下方居中显示“x2000”，
// 文本以反向两侧线性擦除方式显现（从中间两条遮罩向两侧退去）
void showImageAndRevealX2000() {
  int screenW = tft.width();
  int screenH = tft.height();
  int imgX = (screenW - IMG_W) / 2;
  int imgY = (screenH - IMG_H) / 2 + page3ImageYOffset;

  // 图片上移：使用局部重绘（先画新位置，再擦除旧位置底线），保持原始颜色
  int lift = PAGE3_LIFT;
  const int steps = 10;
  // 初始绘制原图
  drawOriginalImageAt(imgX, imgY);
  for (int i = 1; i <= steps; ++i) {
    int yPrevTop = imgY - (i - 1) * lift / steps;
    int yNewTop = imgY - i * lift / steps;
    // 绘制新位置整行（原图）
    drawOriginalImageAt(imgX, yNewTop);
    // 擦除旧位置的底部一行
    int oldBottom = yPrevTop + IMG_H - 1;
    tft.startWrite();
    tft.drawFastHLine(imgX, oldBottom, IMG_W, TFT_BLACK);
    tft.endWrite();
    delay(16);
  }
  page3ImageYOffset = -lift;

    // 计算文本区域
  const char *label = "x2000";
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(4);
  int bw = tft.textWidth(label);
  int bh = tft.fontHeight(4);
  int tx = (screenW - bw) / 2;
  int ty = imgY - lift + IMG_H + 8; // 图片下方 8px

  // 记录文本区域（在动画前记录）
  page3TextX = tx; 
  page3TextY = ty; 
  page3TextW = bw; 
  page3TextH = bh;

  // 两块黑色遮罩从中心向两侧退去（线性动画）
  int cx = tx + bw / 2;
  int maskH = bh;
   
//  // 先完整绘制白色文字
//  tft.setTextColor(TFT_WHITE, TFT_BLACK);
//  tft.setCursor(tx, ty);  // 修正：ty是文字顶部，不需要加bh
//  tft.print(label);
   
  // 然后绘制两块黑色遮罩覆盖文字
  tft.startWrite();
  tft.fillRect(tx, ty, bw/2, maskH, TFT_BLACK);        // 左半遮罩
  tft.fillRect(cx, ty, bw/2, maskH, TFT_BLACK);        // 右半遮罩
  tft.endWrite();
   
  // 最后从中心向两侧逐渐清除遮罩，形成显现动画
  for (int w = 0; w <= bw / 2; w += 2) {  // 步长从4改为3，让动画更细腻
    tft.startWrite();
    // 只清除当前步骤需要变化的区域，实现更精确的局部重绘
    if (w > 0) {
      // 左半部分：只清除新露出的区域
      int leftW = w;
      int leftX = cx - leftW;
      tft.fillRect(leftX, ty, leftW, maskH, TFT_WHITE);
      
      // 右半部分：只清除新露出的区域
      int rightW = w;
      int rightX = cx;
      tft.fillRect(rightX, ty, rightW, maskH, TFT_WHITE);
    }
    tft.endWrite();
    delay(25);  // 延时从14ms增加到25ms，让动画更慢
  }
  
  // 现在文字已经完全显现，白色遮罩覆盖在文字上
  // 需要让白色遮罩逐渐变黑，但文字位置的像素颜色保持不变
  // 这样文字会随着遮罩变黑而越来越清晰
  
  // 先绘制白色文字，记录文字像素位置
  tft.setTextColor(TFT_WHITE, TFT_WHITE);  // 白色文字在白色背景上
  tft.setCursor(tx, ty);
  tft.print(label);
  
  // 使用颜色混合让白色遮罩逐渐变成黑色，形成平滑渐变效果
  // 优化：使用更高效的方法，避免逐像素读取导致的卡死
  int fadeSteps = 16;  // 渐变步数
  int fadeDelay = 20;  // 每步延时
  
  for (int step = 1; step <= fadeSteps; ++step) {
    // 计算当前步骤的颜色：从白色渐变到黑色
    uint16_t currentColor = blend565(TFT_WHITE, TFT_BLACK, step, fadeSteps);
    
    tft.startWrite();
    // 先用渐变颜色覆盖整个文字区域
    tft.fillRect(tx, ty, bw, bh, currentColor);
    // 然后重新绘制白色文字，确保文字像素保持白色
    tft.setTextColor(TFT_WHITE, currentColor);
    tft.setCursor(tx, ty);
    tft.print(label);
    tft.endWrite();
    delay(fadeDelay);
  }
  
  // 最后一步确保完全变成黑色
  tft.startWrite();
  tft.fillRect(tx, ty, bw, bh, TFT_BLACK);
  tft.endWrite();
  
  // 最后重新绘制文字，确保完全清晰
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(tx, ty);
  tft.print(label);
  
  // 调试：打印文字显现动画信息
//  Serial.print("Text reveal: x="); Serial.print(tx); Serial.print(" y="); Serial.print(ty);
//  Serial.print(" w="); Serial.print(bw); Serial.print(" h="); Serial.println(bh);
  // 记录文本区域
  page3TextX = tx; page3TextY = ty; page3TextW = bw; page3TextH = bh;
  // 记录当前的红色质心（在替换前）
  findRedFeatureCenterOnImage(page3RedStartX, page3RedStartY);
  // 然后将图片中的红色替换为白色
  replaceImageRedWithWhite();
}

void transitionToPage3FromPage2() {
  inTransition = true;
  // 顺序调整：先上移图片并显现文本，再移动红色小方块
  page3ImageYOffset = 0;
  showImageAndRevealX2000();
  animateImageToCornerRedSquare();
  currentPage = PAGE_3;
  inTransition = false;
}

// 在 transitionPage3ToPage1 函数之前添加这个新函数
void slideStepToTransition(int newX, int newY, uint16_t color) {
  int dx = newX - posX;
  int dy = newY - posY;

  tft.startWrite();
  // 清除旧位置留下的拖尾区域 - 使用与游戏相同的逻辑
  if (dx != 0) {
    int trailX = dx > 0 ? posX : newX + spriteSize;
    int trailY = posY;
    int trailW = dx > 0 ? dx : -dx;
    int trailH = spriteSize;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  } else if (dy != 0) {
    int trailX = posX;
    int trailY = dy > 0 ? posY : newY + spriteSize;
    int trailW = spriteSize;
    int trailH = dy > 0 ? dy : -dy;
    fillBlackClipped(trailX, trailY, trailW, trailH);
  }

  // 在新位置绘制
  drawSquareAt(newX, newY, color);
  tft.endWrite();

  posX = newX;
  posY = newY;
  delay(8);
}

void transitionPage3ToPage1() {
  inTransition = true;
  // 仅淡出图片与文字区域（保留左上角红色方块）
  int screenW = tft.width();
  int screenH = tft.height();
  int imgX, imgY, iw, ih;
  getImageRect(imgX, imgY, iw, ih);
  int tx = page3TextX, ty = page3TextY, bw = page3TextW, bh = page3TextH;
  
  // 抖动淡出：仅对图片和文字所占行进行覆盖
  int phases = 16, lineDelayMs = 12;
  for (int p = 0; p < phases; ++p) {
    tft.startWrite();
    for (int y = p; y < screenH; y += phases) {
      bool inImg = (y >= imgY && y < imgY + ih);
      bool inText = (tx >= 0 && bw > 0 && bh > 0 && y >= ty && y < ty + bh);
      if (inImg) {
        tft.drawFastHLine(imgX, y, iw, TFT_BLACK);
      }
      if (inText) {
        tft.drawFastHLine(tx, y, bw, TFT_BLACK);
      }
    }
    tft.endWrite();
    delay(lineDelayMs);
  }
  
  // 最终确保图片与文字完全清除
  tft.startWrite();
  tft.fillRect(imgX, imgY, iw, ih, TFT_BLACK);
  if (tx >= 0 && bw > 0 && bh > 0) {
    tft.fillRect(tx, ty, bw, bh, TFT_BLACK);
  }
  tft.endWrite();
  
  // 将左上角格子作为起点，先红->白，再右后下到达中心格，最后白->红
  spriteSize = innerCellSize();
  posX = innerCellX(0);
  posY = innerCellY(0);
  tft.startWrite();
  drawSquareAt(posX, posY, TFT_RED);
  tft.endWrite();
  fadeSquareToWhite();

  int centerRow = GRID_ROWS / 2;
  int centerCol = GRID_COLS / 2;
  int targetX = innerCellX(centerCol);
  int targetY = innerCellY(centerRow);

  // 先向右，再向下 - 使用专门的过渡移动函数
  while (posX != targetX) {   
    int delta = targetX - posX;
    int step = (delta > 0) ? stepPx : -stepPx;
    if (abs(delta) < abs(step)) step = delta;
    slideStepToTransition(posX + step, posY, TFT_WHITE);
  }
  delay(200);
  while (posY != targetY) {  
    int delta = targetY - posY;
    int step = (delta > 0) ? stepPx : -stepPx;
    if (abs(delta) < abs(step)) step = delta;
    slideStepToTransition(posX, posY + step, TFT_WHITE);
  }

  // 白->红
  const uint8_t fadeSteps = 14;
  for (uint8_t i = 1; i <= fadeSteps; ++i) {
    uint16_t c = blend565(TFT_WHITE, TFT_RED, i, fadeSteps);
    tft.startWrite();
    drawSquareAt(posX, posY, c);
    tft.endWrite();
    delay(20);
  }

  curRow = centerRow;
  curCol = centerCol;
  nextMoveAtMs = millis() + random(1000, 2001);
  currentPage = PAGE_GRID;
  inTransition = false;

  page3TextX = page3TextY = -1; page3TextW = page3TextH = 0;
  page3ImageYOffset = 0;
  // 网格逐渐出现
  revealGridLinesGradually();
}

// --- 按键处理 ---
bool readButtonPressed() {
  int v = digitalRead(BUTTON_PIN);
  return v == LOW; // 使用上拉输入，按下为低电平
}

void pollButtonAndHandle() {
  unsigned long now = millis();
  bool pressedNow = readButtonPressed();
  if (pressedNow != buttonLastReadPressed) {
    buttonLastReadPressed = pressedNow;
    buttonLastChangeMs = now;
  }
  if ((now - buttonLastChangeMs) >= BUTTON_DEBOUNCE_MS) {
    if (pressedNow != buttonLastStablePressed) {
      buttonLastStablePressed = pressedNow;
      if (pressedNow) {
        // 按下瞬间
        buttonPressStartMs = now;
        // 游戏中：按下即拍打
        if (currentPage == PAGE_GAME && gameRunning) {
          gameFlap();
        }
      } else {
        // 释放瞬间：短按行为（若不是长按）
        unsigned long held = now - buttonPressStartMs;
        if (held < LONG_PRESS_MS && !inTransition && currentPage != PAGE_GAME) {
          if (currentPage == PAGE_GRID) {
            transitionToPage2();
          } else if (currentPage == PAGE_IMAGE) {
            transitionToPage3FromPage2();
          } else if (currentPage == PAGE_3) {
            transitionPage3ToPage1();
          }
        }
      }
    } else if (pressedNow) {
      // 按住过程中检测长按：仅在 PAGE_GRID 进入游戏（先做过渡动画，再开始）
      if (!inTransition && currentPage == PAGE_GRID && !gameRunning) {
        if ((now - buttonPressStartMs) >= LONG_PRESS_MS) {
          transitionPage1ToGame();
        }
      }
    }
  }
}

// 非阻塞呼吸灯更新
void updateBreathingLed() {
  unsigned long now = millis();
  // 在页面1或游戏时关闭LED，不闪烁
  if (currentPage == PAGE_GRID || currentPage == PAGE_GAME) {
    ledcWrite(LEDC_CHANNEL, 0);
    return;
  }
  if ((now - ledLastUpdateMs) < LED_UPDATE_INTERVAL_MS) return;
  ledLastUpdateMs = now;

  int hwMax = (1 << LEDC_RESOLUTION_BITS) - 1;
  int maxDuty = (long)hwMax * LED_MAX_PERCENT / 100; // 亮度上限
  ledDuty += ledStep;
  if (ledDuty >= maxDuty) {
    ledDuty = maxDuty;
    ledStep = -ledStep;
  } else if (ledDuty <= 0) {
    ledDuty = 0;
    ledStep = -ledStep;
  }
  ledcWrite(LEDC_CHANNEL, ledDuty);
}

void setup() {
  tft.init();
  tft.setRotation(0); // 根据安装方向可改为 0/1/2/3
  computeGridGeometry();
  // 启动动画依次：显示图片、抖动渐黑、红方块闪烁、网格显现
  randomSeed(esp_random());
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // 初始化呼吸灯 PWM
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RESOLUTION_BITS);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, 0);
  
  // 初始化掉电保存并加载最高分
  preferences.begin("flappy", false);
  loadHighScore();
  
  runBootAnimation();

  nextMoveAtMs = millis() + random(1000, 2001); // 1-2 秒后开始移动
}

void loop() {
  unsigned long now = millis();
  // 每帧轮询按键
  pollButtonAndHandle();
  // 呼吸灯更新（非阻塞）
  updateBreathingLed();
  // 游戏页面更新
  if (currentPage == PAGE_GAME) {
    updateGame();
    return;
  }
  if (currentPage == PAGE_GRID && !inTransition && (long)(now - nextMoveAtMs) >= 0) {
    int targetRow, targetCol;
    // 选择与当前不同的目标格
    do {
      targetRow = random(0, GRID_ROWS);
      targetCol = random(0, GRID_COLS);
    } while (targetRow == curRow && targetCol == curCol);

    animateMoveTo(targetRow, targetCol, TFT_RED);

    nextMoveAtMs = millis() + random(1000, 3001); // 下一次移动的等待时间（2-5 秒）
  }
  // 页面3没有持续动画，等待按键触发返回
}
