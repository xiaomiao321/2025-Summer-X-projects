#include "Menu.h"
#include "RotaryEncoder.h"

// 全局变量定义（在 Main.ino 中定义，这里仅声明）
extern TFT_eSPI tft;
extern int16_t display;
extern uint8_t picture_flag;
extern int16_t display_trg;
extern uint8_t circle_num;
extern const char *words[];
extern const int speed_choose;
extern const int icon_size;

// -----------------------------
// 动画平滑移动函数
// -----------------------------
void ui_run(int16_t *a, int16_t *a_trg, int b) {
  if (*a < *a_trg) {
    *a += b;
    if (*a > *a_trg) *a = *a_trg;
  }
  if (*a > *a_trg) {
    *a -= b;
    if (*a < *a_trg) *a = *a_trg;
  }
}

// -----------------------------
// 向右移动图标
// -----------------------------
void ui_right_one_Picture(int16_t *a, int b) {
  if (*a <= 48) {
    *a += b;
  } else {
    *a = -144 + b;
  }
}

// -----------------------------
// 向左移动图标
// -----------------------------
void ui_left_one_Picture(int16_t *a, int b) {
  if (*a >= -144) {
    *a -= b;
  } else {
    *a = 96 - b;
  }
}

// -----------------------------
// 通用子菜单进入动画
// -----------------------------
void toSubMenuDisplay(const char *title) {
  int16_t menu_display = 10, menu_display_trg = 74;
  while (menu_display != menu_display_trg) {
    tft.fillRect(0, 5, 128, 100, TFT_BLACK); // 清除动画区域
    tft.drawString("MENU:", 52, menu_display);
    tft.fillTriangle(64, menu_display + 26, 84, menu_display + 16, 84, menu_display + 36, TFT_WHITE);
    tft.fillRect(display, menu_display + 6, icon_size, icon_size, TFT_GREEN);
    tft.fillRect(display + 48, menu_display + 6, icon_size, icon_size, TFT_BLUE);
    tft.fillRect(display + 96, menu_display + 6, icon_size, icon_size, TFT_RED);
    tft.fillRect(display + 144, menu_display + 6, icon_size, icon_size, TFT_YELLOW);
    tft.fillRect(display + 192, menu_display + 6, icon_size, icon_size, TFT_CYAN);
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

// -----------------------------
// 通用子菜单返回动画
// -----------------------------
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
    tft.fillRect(0, 5, 128, 100, TFT_BLACK); // 清除动画区域
    tft.drawString("MENU:", 52, menu_display);
    tft.fillTriangle(64, menu_display + 26, 84, menu_display + 16, 84, menu_display + 36, TFT_WHITE);
    tft.fillRect(display, menu_display + 6, icon_size, icon_size, TFT_GREEN);
    tft.fillRect(display + 48, menu_display + 6, icon_size, icon_size, TFT_BLUE);
    tft.fillRect(display + 96, menu_display + 6, icon_size, icon_size, TFT_RED);
    tft.fillRect(display + 144, menu_display + 6, icon_size, icon_size, TFT_YELLOW);
    tft.fillRect(display + 192, menu_display + 6, icon_size, icon_size, TFT_CYAN);
    ui_run(&menu_display, &menu_display_trg, 8);
    delay(10);
  }
}

// -----------------------------
// 显示主菜单配置
// -----------------------------
void showMenuConfig() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.fillTriangle(64, 80, 84, 70, 84, 90, TFT_WHITE);
  tft.fillRect(display, 20, icon_size, icon_size, TFT_GREEN);
  tft.fillRect(display + 48, 20, icon_size, icon_size, TFT_BLUE);
  tft.fillRect(display + 96, 20, icon_size, icon_size, TFT_RED);
  tft.fillRect(display + 144, 20, icon_size, icon_size, TFT_YELLOW);
  tft.fillRect(display + 192, 20, icon_size, icon_size, TFT_CYAN);
  tft.drawString("MENU:", 52, 5);
  tft.drawString(words[picture_flag], 82, 5);
}

// -----------------------------
// 游戏菜单
// -----------------------------
void gameMenu() {
  toSubMenuDisplay("GAME");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Game Menu", tft.width() / 2, tft.height() / 2);
  while (1) {
    if (readButton()) {
      subToMenuDisplay("GAME");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}

// -----------------------------
// 消息菜单
// -----------------------------
void messageMenu() {
  toSubMenuDisplay("MESSAGE");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Message Menu", tft.width() / 2, tft.height() / 2);
  while (1) {
    if (readButton()) {
      subToMenuDisplay("MESSAGE");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}

// -----------------------------
// 设置菜单
// -----------------------------
void settingMenu() {
  toSubMenuDisplay("SETTING");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Setting Menu", tft.width() / 2, tft.height() / 2);
  while (1) {
    if (readButton()) {
      subToMenuDisplay("SETTING");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}

// -----------------------------
// 显示主菜单并处理导航
// -----------------------------
void showMenu() {
  int direction = readEncoder();
  if (direction == 1 && display > -144) {
    picture_flag = (picture_flag + 1) % 5;
    circle_num = 48 / speed_choose;
    while (circle_num) {
      tft.fillRect(0, 5, 128, 100, TFT_BLACK);
      ui_left_one_Picture(&display, speed_choose);
      tft.fillTriangle(64, 80, 84, 70, 84, 90, TFT_WHITE);
      tft.fillRect(display, 20, icon_size, icon_size, TFT_GREEN);
      tft.fillRect(display + 48, 20, icon_size, icon_size, TFT_BLUE);
      tft.fillRect(display + 96, 20, icon_size, icon_size, TFT_RED);
      tft.fillRect(display + 144, 20, icon_size, icon_size, TFT_YELLOW);
      tft.fillRect(display + 192, 20, icon_size, icon_size, TFT_CYAN);
      tft.drawString("MENU:", 52, 5);
      tft.drawString(words[picture_flag], 82, 5);
      circle_num--;
      delay(10);
    }
  } else if (direction == -1 && display < 48) {
    picture_flag = (picture_flag == 0) ? 4 : picture_flag - 1;
    circle_num = 48 / speed_choose;
    while (circle_num) {
      tft.fillRect(0, 5, 128, 100, TFT_BLACK);
      ui_right_one_Picture(&display, speed_choose);
      tft.fillTriangle(64, 80, 84, 70, 84, 90, TFT_WHITE);
      tft.fillRect(display, 20, icon_size, icon_size, TFT_GREEN);
      tft.fillRect(display + 48, 20, icon_size, icon_size, TFT_BLUE);
      tft.fillRect(display + 96, 20, icon_size, icon_size, TFT_RED);
      tft.fillRect(display + 144, 20, icon_size, icon_size, TFT_YELLOW);
      tft.fillRect(display + 192, 20, icon_size, icon_size, TFT_CYAN);
      tft.drawString("MENU:", 52, 5);
      tft.drawString(words[picture_flag], 82, 5);
      circle_num--;
      delay(10);
    }
  } else {
    tft.fillRect(82, 5, 46, 10, TFT_BLACK);
    tft.drawString(words[picture_flag], 82, 5);
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