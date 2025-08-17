#include "Menu.h"
#include "RotaryEncoder.h"

// TFT 初始化
TFT_eSPI tft = TFT_eSPI();

// 菜单相关变量
static int16_t display = 48;      // 图标显示位置
static int16_t display_trg = 1;   // 目标位置
static uint8_t picture_flag = 0;  // 功能选择标志位
static uint8_t game_menu_flag = 0;// 游戏菜单标志位
static uint8_t circle_num;        // 动画循环次数
static const char *words[] = {"GAME", "MESSAGE", "SETTING"};
static const int speed_choose = 4; // 动画速度（步进像素）
static const int icon_size = 48;  // 增大图标大小到48x48

// 动画平滑移动函数
void ui_run(int16_t *a, int16_t *a_trg, int b) {
  if (*a < *a_trg) {
    *a += b;
    if (*a > *a_trg) *a = *a_trg; // 防止加过头
  }
  if (*a > *a_trg) {
    *a -= b;
    if (*a < *a_trg) *a = *a_trg; // 防止减过头
  }
}

// 向右移动图标
void ui_right_one_Picture(int16_t *a, int b) {
  if (*a <= 48) {
    *a += b;
  } else {
    *a = -96 + b; // 循环重置位置，实现无缝循环
  }
}

// 向左移动图标
void ui_left_one_Picture(int16_t *a, int b) {
  if (*a >= -48) {
    *a -= b;
  } else {
    *a = 96 - b; // 循环重置位置，实现无缝循环
  }
}
// 通用子菜单进入动画
void toSubMenuDisplay(const char *title) {
  int16_t menu_display = 10, menu_display_trg = 74;
  while (menu_display != menu_display_trg) {
    tft.fillRect(0, 5, 128, 70, TFT_BLACK); // 清除动画区域
    tft.drawString("MENU:", 52, menu_display);
    tft.fillTriangle(64, menu_display + 26, 84, menu_display + 16, 84, menu_display + 36, TFT_WHITE);
    tft.fillRect(display, menu_display + 6, icon_size, icon_size, TFT_GREEN);
    tft.fillRect(display + 48, menu_display + 6, icon_size, icon_size, TFT_BLUE);
    tft.fillRect(display + 96, menu_display + 6, icon_size, icon_size, TFT_RED);
    ui_run(&menu_display, &menu_display_trg, 8);
    delay(10);
  }

  menu_display = 74;
  menu_display_trg = 10;
  while (menu_display != menu_display_trg) {
    tft.fillRect(0, 10, 128, 60, TFT_BLACK); // 清除动画区域
    tft.drawString(title, 52, menu_display);
    tft.drawLine(1, menu_display + 3, 128, menu_display + 3, TFT_WHITE);
    ui_run(&menu_display, &menu_display_trg, 8);
    delay(10);
  }
}


// 通用子菜单返回动画
void subToMenuDisplay(const char *title) {
  int16_t menu_display = 10, menu_display_trg = 74;
  while (menu_display != menu_display_trg) {
    tft.fillRect(0, 10, 128, 60, TFT_BLACK); // 清除动画区域
    tft.drawString(title, 52, menu_display);
    tft.drawLine(1, menu_display + 3, 128, menu_display + 3, TFT_WHITE);
    ui_run(&menu_display, &menu_display_trg, 8);
    delay(10);
  }

  menu_display = 74;
  menu_display_trg = 10;
  while (menu_display != menu_display_trg) {
    tft.fillRect(0, 5, 128, 70, TFT_BLACK); // 清除动画区域
    tft.drawString("MENU:", 52, menu_display);
    tft.fillTriangle(64, menu_display + 26, 84, menu_display + 16, 84, menu_display + 36, TFT_WHITE);
    tft.fillRect(display, menu_display + 6, icon_size, icon_size, TFT_GREEN);
    tft.fillRect(display + 48, menu_display + 6, icon_size, icon_size, TFT_BLUE);
    tft.fillRect(display + 96, menu_display + 6, icon_size, icon_size, TFT_RED);
    ui_run(&menu_display, &menu_display_trg, 8);
    delay(10);
  }
}
// 显示主菜单配置
static void showMenuConfig() {
  tft.fillScreen(TFT_BLACK); // 仅初始化时清屏
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  // 绘制箭头和图标（增大图标，避免重叠）
  tft.fillTriangle(64, 56, 84, 46, 84, 66, TFT_WHITE); // 箭头位置调整
  tft.fillRect(display, 20, icon_size, icon_size, TFT_GREEN);        // 游戏图标
  tft.fillRect(display + 48, 20, icon_size, icon_size, TFT_BLUE);    // 消息图标
  tft.fillRect(display + 96, 20, icon_size, icon_size, TFT_RED);     // 设置图标
  tft.drawString("MENU:", 52, 5); // 文本上移，避免重叠
  tft.drawString(words[picture_flag], 82, 5); // 显示当前选项，上移
}

// 初始化 TFT 显示屏和菜单
void initMenu() {
  tft.init();
  tft.setRotation(1); // 调整屏幕方向（根据需要设置为 0-3）
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  // 显示初始菜单
  showMenuConfig();
}



// 游戏菜单
void gameMenu() {
  static int8_t box_flag = 0;
  static int16_t box_x = 1, box_y = 14, box_w = 84, box_h = 13;
  static int16_t box_x_trg, box_y_trg, box_w_trg, box_h_trg;

  toGameMenuDisplay();
  while (1) {
    int direction = readEncoder();
    tft.fillRect(0, 10, 128, 50, TFT_BLACK); // 清除游戏菜单区域
    tft.drawString("GAME", 52, 10);
    tft.drawLine(1, 13, 128, 13, TFT_WHITE);
    tft.drawString("game1:FlyBird", 3, 24);
    tft.drawString("game2:Dinosaur Rex", 3, 36);
    tft.drawString("game3:Stick Fight", 3, 48);
    tft.drawString("game4:Tetris", 3, 60);
    
    if (direction == 1) {
      box_flag = (box_flag >= 3) ? 3 : box_flag + 1;
    } else if (direction == -1) {
      box_flag = (box_flag <= 0) ? 0 : box_flag - 1;
    }

    // 设置框的目标值
    switch (box_flag) {
      case 0: box_x_trg = 1; box_y_trg = 14; box_w_trg = 84; box_h_trg = 13; break;
      case 1: box_x_trg = 1; box_y_trg = 27; box_w_trg = 108; box_h_trg = 13; break;
      case 2: box_x_trg = 1; box_y_trg = 38; box_w_trg = 96; box_h_trg = 13; break;
      case 3: box_x_trg = 1; box_y_trg = 51; box_w_trg = 72; box_h_trg = 12; break;
    }

    ui_run(&box_x, &box_x_trg, 1);
    ui_run(&box_y, &box_y_trg, 1);
    ui_run(&box_w, &box_w_trg, 2);
    ui_run(&box_h, &box_h_trg, 1);
    tft.drawRect(box_x, box_y, box_w, box_h, TFT_YELLOW);

    if (readButton()) {
      gameToMenuDisplay();
      break;
    }
    delay(10);
  }

  // 重置状态
  game_menu_flag = 0;
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}

// 测试消息子菜单
void messageMenu() {
  static int8_t box_flag = 0;
  static int16_t box_x = 1, box_y = 14, box_w = 84, box_h = 13;
  static int16_t box_x_trg, box_y_trg, box_w_trg, box_h_trg;

  toSubMenuDisplay("MESSAGE");
  while (1) {
    int direction = readEncoder();
    tft.fillRect(0, 10, 128, 50, TFT_BLACK); // 清除菜单区域
    tft.drawString("MESSAGE", 52, 10);
    tft.drawLine(1, 13, 128, 13, TFT_WHITE);
    tft.drawString("Msg1: Test1", 3, 24);
    tft.drawString("Msg2: Test2", 3, 36);
    tft.drawString("Msg3: Test3", 3, 48);
    
    if (direction == 1) {
      box_flag = (box_flag >= 2) ? 2 : box_flag + 1;
    } else if (direction == -1) {
      box_flag = (box_flag <= 0) ? 0 : box_flag - 1;
    }

    switch (box_flag) {
      case 0: box_x_trg = 1; box_y_trg = 14; box_w_trg = 72; box_h_trg = 13; break;
      case 1: box_x_trg = 1; box_y_trg = 27; box_w_trg = 72; box_h_trg = 13; break;
      case 2: box_x_trg = 1; box_y_trg = 38; box_w_trg = 72; box_h_trg = 13; break;
    }

    ui_run(&box_x, &box_x_trg, 1);
    ui_run(&box_y, &box_y_trg, 1);
    ui_run(&box_w, &box_w_trg, 2);
    ui_run(&box_h, &box_h_trg, 1);
    tft.drawRect(box_x, box_y, box_w, box_h, TFT_YELLOW);

    if (readButton()) {
      subToMenuDisplay("MESSAGE");
      break;
    }
    delay(10);
  }

  display = 48;
  picture_flag = 0;
  showMenuConfig();
}

// 测试设置子菜单
void settingMenu() {
  static int8_t box_flag = 0;
  static int16_t box_x = 1, box_y = 14, box_w = 84, box_h = 13;
  static int16_t box_x_trg, box_y_trg, box_w_trg, box_h_trg;

  toSubMenuDisplay("SETTING");
  while (1) {
    int direction = readEncoder();
    tft.fillRect(0, 10, 128, 50, TFT_BLACK); // 清除菜单区域
    tft.drawString("SETTING", 52, 10);
    tft.drawLine(1, 13, 128, 13, TFT_WHITE);
    tft.drawString("Set1: Brightness", 3, 24);
    tft.drawString("Set2: Volume", 3, 36);
    tft.drawString("Set3: Language", 3, 48);
    
    if (direction == 1) {
      box_flag = (box_flag >= 2) ? 2 : box_flag + 1;
    } else if (direction == -1) {
      box_flag = (box_flag <= 0) ? 0 : box_flag - 1;
    }

    switch (box_flag) {
      case 0: box_x_trg = 1; box_y_trg = 14; box_w_trg = 96; box_h_trg = 13; break;
      case 1: box_x_trg = 1; box_y_trg = 27; box_w_trg = 72; box_h_trg = 13; break;
      case 2: box_x_trg = 1; box_y_trg = 38; box_w_trg = 84; box_h_trg = 13; break;
    }

    ui_run(&box_x, &box_x_trg, 1);
    ui_run(&box_y, &box_y_trg, 1);
    ui_run(&box_w, &box_w_trg, 2);
    ui_run(&box_h, &box_h_trg, 1);
    tft.drawRect(box_x, box_y, box_w, box_h, TFT_YELLOW);

    if (readButton()) {
      subToMenuDisplay("SETTING");
      break;
    }
    delay(10);
  }

  display = 48;
  picture_flag = 0;
  showMenuConfig();
}





// 修改 toGameMenuDisplay 和 gameToMenuDisplay 为调用通用函数
void toGameMenuDisplay() {
  toSubMenuDisplay("GAME");
}

void gameToMenuDisplay() {
  subToMenuDisplay("GAME");
}

// 显示主菜单
void showMenu() {
  int direction = readEncoder();
  if (direction == 1 && display > -48) { // 向左滑动
    picture_flag = (picture_flag + 1) % 3;
    circle_num = 48 / speed_choose;
    while (circle_num) {
      tft.fillRect(0, 5, 128, 70, TFT_BLACK); // 清除图标和文本区域
      ui_left_one_Picture(&display, speed_choose);
      tft.fillTriangle(64, 56, 84, 46, 84, 66, TFT_WHITE);
      tft.fillRect(display, 20, icon_size, icon_size, TFT_GREEN);
      tft.fillRect(display + 48, 20, icon_size, icon_size, TFT_BLUE);
      tft.fillRect(display + 96, 20, icon_size, icon_size, TFT_RED);
      tft.drawString("MENU:", 52, 5);
      tft.drawString(words[picture_flag], 82, 5);
      circle_num--;
      delay(10);
    }
  } else if (direction == -1 && display < 48) { // 向右滑动
    picture_flag = (picture_flag == 0) ? 2 : picture_flag - 1;
    circle_num = 48 / speed_choose;
    while (circle_num) {
      tft.fillRect(0, 5, 128, 70, TFT_BLACK); // 清除图标和文本区域
      ui_right_one_Picture(&display, speed_choose);
      tft.fillTriangle(64, 56, 84, 46, 84, 66, TFT_WHITE);
      tft.fillRect(display, 20, icon_size, icon_size, TFT_GREEN);
      tft.fillRect(display + 48, 20, icon_size, icon_size, TFT_BLUE);
      tft.fillRect(display + 96, 20, icon_size, icon_size, TFT_RED);
      tft.drawString("MENU:", 52, 5);
      tft.drawString(words[picture_flag], 82, 5);
      circle_num--;
      delay(10);
    }
  } else {
    tft.fillRect(82, 5, 46, 10, TFT_BLACK); // 清除旧文本
    tft.drawString(words[picture_flag], 82, 5);
  }

  if (readButton()) { // 按下按钮进入对应子菜单
    switch (picture_flag) {
      case 0: gameMenu(); break;
      case 1: messageMenu(); break;
      case 2: settingMenu(); break;
    }
  }
}

