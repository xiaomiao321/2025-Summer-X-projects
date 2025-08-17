#include "Menu.h"

// TFT 初始化
TFT_eSPI tft = TFT_eSPI();

// 菜单相关变量
static int currentMenu = 0;          // 当前菜单索引 (0: 主菜单)
static int selectedOption = 0;       // 当前选中的菜单项
static const int numMenuItems = 4;  // 主菜单项数量
static int settings[3] = {0, 0, 0};  // 子菜单设置值（音量、亮度、模式）
static const char *mainMenu[] = {"Volume", "Brightness", "Mode", "Exit"};
static const int maxSettings[3] = {100, 255, 3};  // 每个设置的最大值


// 显示主菜单
static void displayMenu() {
  tft.fillScreen(TFT_BLACK); // 清屏
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 10);
  tft.println("Main Menu");

  for (int i = 0; i < numMenuItems; i++) {
    tft.setCursor(20, 40 + i * 30);
    if (i == selectedOption) {
      tft.setTextColor(TFT_YELLOW, TFT_DARKGREY); // 高亮选中项
      tft.fillRect(10, 35 + i * 30, tft.width() - 20, 25, TFT_DARKGREY);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tft.println(mainMenu[i]);
  }
}

// 初始化 TFT 显示屏和菜单
void initMenu() {
  tft.init();
  tft.setRotation(1); // 调整屏幕方向（根据需要设置为 0-3）
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  // 显示初始菜单
  displayMenu();
}


// 显示子菜单
static void displaySubMenu(int index) {
  tft.fillScreen(TFT_BLACK); // 清屏
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 10);
  tft.print("Set ");
  tft.println(mainMenu[index]);

  tft.setCursor(10, 50);
  tft.print("Value: ");
  tft.println(settings[index]);
}

// 更新菜单
void updateMenu(int direction) {
  if (currentMenu == 0) { // 主菜单
    selectedOption = (selectedOption + direction + numMenuItems) % numMenuItems;
    displayMenu();
  } else { // 子菜单（调整设置值）
    int index = currentMenu - 1;
    settings[index] = constrain(settings[index] + direction, 0, maxSettings[index]);
    displaySubMenu(index);
  }
}

// 处理按钮按下事件
void handleButtonPress() {
  if (currentMenu == 0) { // 主菜单
    if (selectedOption == 3) { // Exit
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(10, 10);
      tft.println("Exiting Menu");
      delay(1000); // 短暂显示退出信息
      // 可以在这里添加退出逻辑
    } else {
      currentMenu = selectedOption + 1; // 进入子菜单
      displaySubMenu(selectedOption);
    }
  } else { // 子菜单
    currentMenu = 0; // 返回主菜单
    displayMenu();
  }
}