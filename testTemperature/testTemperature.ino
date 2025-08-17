#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("ESP32-C3 温度测试开始");
}

void loop() {
  float temp = temperatureRead();
  char buf[32];
  snprintf(buf, sizeof(buf), "ESP32C3_TEMP:%.1f\n", temp);
  Serial.print(buf);
  delay(1000);
}