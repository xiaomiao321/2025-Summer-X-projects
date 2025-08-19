#include <Arduino.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>

// 初始化 TFT
TFT_eSPI tft = TFT_eSPI();

// 图像参数（与主项目一致）
#define IMAGE_WIDTH 40
#define IMAGE_HEIGHT 40
#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT * 2) // 3200 bytes (RGB565)
#define LOGO_X 10      // 参考 performance.cpp
#define LOGO_Y_TOP 24
#define LOGO_Y_BOTTOM 64

void setup() {
  // 初始化串口
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // 初始化 TFT
  tft.init();
  tft.setRotation(1); // 横屏 128x128
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SPIFFS Test", 0, 0);

  // 初始化 SPIFFS
  Serial.println("初始化 SPIFFS...");
  if (!SPIFFS.begin(true)) { // true 表示自动格式化
    Serial.println("SPIFFS 初始化失败！");
    tft.drawString("SPIFFS 失败", 0, 20);
    return;
  }
  Serial.println("SPIFFS 初始化成功");
  tft.drawString("SPIFFS OK", 0, 20);

  // 写入测试图像文件
  writeTestImage("/nvidia.bin");
  writeTestImage("/intelcore.bin");

  // 显示图像
  displayImage("/nvidia.bin", LOGO_X, LOGO_Y_TOP);
  displayImage("/intelcore.bin", LOGO_X, LOGO_Y_BOTTOM);

  // 列出 SPIFFS 文件
  listFiles();
}

void loop() {
  // 定期打印 SPIFFS 使用情况并更新屏幕状态
  Serial.printf("SPIFFS 总空间: %u 字节, 已使用: %u 字节\n", 
                SPIFFS.totalBytes(), SPIFFS.usedBytes());
  tft.fillRect(0, 40, 128, 20, TFT_BLACK); // 清除状态区域
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Running...", 0, 40);
  delay(1000); // 每秒更新
}

// 写入测试图像文件
void writeTestImage(const char* path) {
  // 模拟图像数据（NVIDIA: 绿色 0x07E0, Intel: 蓝色 0x001F）
  uint16_t imageData[IMAGE_WIDTH * IMAGE_HEIGHT];
  uint16_t color = (strcmp(path, "/nvidia.bin") == 0) ? 0x07E0 : 0x001F;
  for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
    imageData[i] = color;
  }

  File file = SPIFFS.open(path, "w");
  if (!file) {
    Serial.printf("无法创建文件: %s\n", path);
    tft.drawString("写失败: " + String(path), 0, 60);
    return;
  }

  size_t written = file.write((uint8_t*)imageData, IMAGE_SIZE);
  file.close();

  if (written == IMAGE_SIZE) {
    Serial.printf("成功写入 %s, 大小: %u 字节\n", path, written);
  } else {
    Serial.printf("写入 %s 失败, 写入: %u 字节\n", path, written);
    tft.drawString("写失败: " + String(path), 0, 60);
  }
}

// 从 SPIFFS 读取并显示图像
void displayImage(const char* path, int16_t x, int16_t y) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.printf("无法打开文件: %s\n", path);
    tft.fillRect(x, y, IMAGE_WIDTH, IMAGE_HEIGHT, TFT_RED); // 错误显示红色矩形
    tft.drawString("读失败: " + String(path), 0, 80);
    return;
  }

  if (file.size() != IMAGE_SIZE) {
    Serial.printf("文件 %s 大小错误: %u 字节 (预期 %u)\n", path, file.size(), IMAGE_SIZE);
    tft.fillRect(x, y, IMAGE_WIDTH, IMAGE_HEIGHT, TFT_RED);
    tft.drawString("大小错误: " + String(path), 0, 80);
    file.close();
    return;
  }

  // 读取图像数据
  uint16_t buffer[IMAGE_WIDTH * IMAGE_HEIGHT];
  file.read((uint8_t*)buffer, IMAGE_SIZE);
  file.close();

  // 显示图像
  tft.pushImage(x, y, IMAGE_WIDTH, IMAGE_HEIGHT, buffer);
  Serial.printf("显示 %s at (%d, %d)\n", path, x, y);
}

// 列出 SPIFFS 中的所有文件
void listFiles() {
  Serial.println("SPIFFS 文件列表:");
  tft.fillRect(0, 100, 128, 28, TFT_BLACK); // 清除文件列表区域
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  int y = 100;
  while (file) {
    Serial.printf("文件: %s, 大小: %u 字节\n", file.name(), file.size());
    tft.drawString(String(file.name()) + ": " + String(file.size()) + "B", 0, y);
    y += 10;
    file = root.openNextFile();
  }
  root.close();
}