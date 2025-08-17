#include "RotaryEncoder.h"
#include "Menu.h"

void setup() {
  initRotaryEncoder();
  initMenu();
}

void loop() {
  // 处理旋转编码器
  int direction = readEncoder();
  if (direction != 0) {
    updateMenu(direction);
  }

  // 处理按钮
  if (readButton()) {
    handleButtonPress();
  }
}