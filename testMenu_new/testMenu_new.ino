#include "RotaryEncoder.h"
#include "Menu.h"

void setup() {
  initRotaryEncoder();
  initMenu();
}

void loop() {
  showMenu();
  delay(10); // 控制循环速度
}