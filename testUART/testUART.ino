#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft;  // 创建 TFT 对象

#define BUFFER_SIZE 512
char inputBuffer[BUFFER_SIZE];
uint16_t bufferIndex = 0;
bool stringComplete = false;

// 显示区域设置
#define LINE_HEIGHT 18
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define ERROR_COLOR TFT_RED
#define VALUE_COLOR TFT_CYAN

void setup() {
  // 初始化TFT屏幕
  tft.init();
  tft.setRotation(1); // 横屏，根据需要调整：0~3
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR, BG_COLOR); // 前景色和背景色都设置
  tft.setTextSize(1);
  tft.setFreeFont(&FreeSans9pt7b); // 可选字体，确保已启用

  // 打印启动信息
  tft.setCursor(0, 0);
  tft.setTextColor(TITLE_COLOR);
  tft.println("ESP32 Monitor");
  tft.setTextColor(TEXT_COLOR);
  tft.println("Waiting for data...");

  // 初始化串口0
  Serial.begin(115200);
}

void loop() {
  // 检查串口是否有数据
  if (Serial.available()) {
    char inChar = (char)Serial.read();

    if (bufferIndex < BUFFER_SIZE - 1) {
      inputBuffer[bufferIndex++] = inChar;
      inputBuffer[bufferIndex] = '\0';
    } else {
      // 缓冲区满
      showError("Buffer Full!");
      resetBuffer();
    }

    // 检测换行符
    if (inChar == '\n') {
      stringComplete = true;
    }
  }

  // 处理完整数据
    if (stringComplete) {
      parseAndDisplayData();
      delay(3000);

      // ✅ 只清除数据区域，不全屏清
      tft.fillRect(0, 0, tft.width(), tft.height(), BG_COLOR);

      tft.setCursor(0, 0);
      tft.setTextColor(TITLE_COLOR, BG_COLOR);
      tft.println("ESP32 Monitor");
      tft.setTextColor(TEXT_COLOR, BG_COLOR);
      tft.println("Waiting for data...");

      stringComplete = false;
      resetBuffer();
    }
}

// 解析并显示数据（核心函数）
void parseAndDisplayData() {
  tft.setCursor(0, 0);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.println("Received Data:");
  tft.println(""); // 空行
  tft.setTextColor(TEXT_COLOR, BG_COLOR); // 设置默认文字颜色+背景


  // ========== 数据变量定义（保持不变）==========
  char cpuName[64] = "Unknown";
  char gpuName[64] = "Unknown";
  char fan[16] = "Unknown";
  char rpm[16] = "Unknown";
  int cpuTemp = 0, cpuLoad = 0, cpuFreq = 0;
  int gpuTemp = 0, gpuLoad = 0, gpuFreq = 0;
  int gpuMemClock = 0, gpuShaderClock = 0;
  float ramTotal = 0.0, ramActive = 0.0, ramLoad = 0;
  int memTotal = 0, memUsed = 0, memLoad = 0;
  float power = 0.0;

  // ========== 解析逻辑（完全保留，未修改）==========
  char *ptr = nullptr;

  // 解析CPU温度和负载
  ptr = strstr(inputBuffer, "C");
  if (ptr) {
    char *degree = strchr(ptr, 'c');
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      cpuTemp = atoi(temp);
      cpuLoad = atoi(load);
    }
  }

  // 解析GPU温度和负载
  ptr = strstr(inputBuffer, "G");
  if (ptr) {
    char *degree = strchr(ptr, 'c');
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      gpuTemp = atoi(temp);
      gpuLoad = atoi(load);
    }
  }

  // 解析RAM总量
  ptr = strstr(inputBuffer, "R");
  if (ptr) {
    char *end = strchr(ptr, 'G');
    if (end) {
      char ram[8] = {0};
      strncpy(ram, ptr + 1, end - ptr - 1);
      ramTotal = atof(ram);
    }
  }

  // 解析RAM活跃
  ptr = strstr(inputBuffer, "RA");
  if (ptr) {
    ptr += 2;
    char *end = strchr(ptr, '|');
    if (end) {
      char ram[8] = {0};
      strncpy(ram, ptr, end - ptr);
      ramActive = atof(ram);
    }
  }

  // 解析RAM负载
  ptr = strstr(inputBuffer, "RL");
  if (ptr) {
    ptr += 2;
    char *end = strchr(ptr, '|');
    if (end) {
      char load[8] = {0};
      strncpy(load, ptr, end - ptr);
      ramLoad = atof(load);
    }
  }

  // 解析内存总量
  ptr = strstr(inputBuffer, "GMT");
  if (ptr) {
    ptr += 3;
    char *end = strchr(ptr, '|');
    if (end) {
      char mem[8] = {0};
      strncpy(mem, ptr, end - ptr);
      memTotal = atoi(mem);
    }
  }

  // 解析内存使用
  ptr = strstr(inputBuffer, "GMU");
  if (ptr) {
    ptr += 3;
    char *end = strchr(ptr, '|');
    if (end) {
      char mem[8] = {0};
      strncpy(mem, ptr, end - ptr);
      memUsed = atoi(mem);
    }
  }

  // 解析内存负载
  ptr = strstr(inputBuffer, "GML");
  if (ptr) {
    ptr += 3;
    char *end = strchr(ptr, '|');
    if (end) {
      char load[8] = {0};
      strncpy(load, ptr, end - ptr);
      memLoad = atoi(load);
    }
  }

  // 解析风扇
  ptr = strstr(inputBuffer, "GFANL");
  if (ptr) {
    ptr += 5;
    char *end = strchr(ptr, '|');
    if (end && end - ptr < sizeof(fan) - 1) {
      strncpy(fan, ptr, end - ptr);
      fan[end - ptr] = '\0';
    }
  }

  // 解析RPM
  ptr = strstr(inputBuffer, "GRPM");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (end && end - ptr < sizeof(rpm) - 1) {
      strncpy(rpm, ptr, end - ptr);
      rpm[end - ptr] = '\0';
    }
  }

  // 解析功率
  ptr = strstr(inputBuffer, "GPWR");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (end) {
      char pwr[8] = {0};
      strncpy(pwr, ptr, end - ptr);
      power = atof(pwr);
    }
  }

  // 解析CPU名称
  ptr = strstr(inputBuffer, "CPU:");
  if (ptr) {
    ptr += 4;
    char *end = strstr(ptr, "GPU:");
    if (!end) end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(cpuName) - 1) {
      strncpy(cpuName, ptr, end - ptr);
      cpuName[end - ptr] = '\0';
      for (int i = strlen(cpuName) - 1; i >= 0 && cpuName[i] == ' '; i--) cpuName[i] = '\0';
    }
  }

  // 解析GPU名称
  ptr = strstr(inputBuffer, "GPU:");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (!end) end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(gpuName) - 1) {
      strncpy(gpuName, ptr, end - ptr);
      gpuName[end - ptr] = '\0';
      for (int i = strlen(gpuName) - 1; i >= 0 && gpuName[i] == ' '; i--) gpuName[i] = '\0';
    }
  }

  // 解析GPU频率
  ptr = strstr(inputBuffer, "GCC");
  if (ptr) {
    ptr += 3;
    char *end = strchr(ptr, '|');
    if (end) {
      char freq[8] = {0};
      strncpy(freq, ptr, end - ptr);
      gpuFreq = atoi(freq);
    }
  }

  // 解析GPU内存时钟
  ptr = strstr(inputBuffer, "GMC");
  if (ptr) {
    ptr += 3;
    char *end = strchr(ptr, '|');
    if (end) {
      char memclk[8] = {0};
      strncpy(memclk, ptr, end - ptr);
      gpuMemClock = atoi(memclk);
    }
  }

  // 解析GPU着色器时钟
  ptr = strstr(inputBuffer, "GSC");
  if (ptr) {
    ptr += 3;
    char *end = strchr(ptr, '|');
    if (end) {
      char sclk[8] = {0};
      strncpy(sclk, ptr, end - ptr);
      gpuShaderClock = atoi(sclk);
    }
  }

  // 解析CPU频率
  ptr = strstr(inputBuffer, "CHC");
  if (ptr) {
    ptr += 3;
    char *end = strchr(ptr, '|');
    if (end) {
      char freq[8] = {0};
      strncpy(freq, ptr, end - ptr);
      cpuFreq = atoi(freq);
    }
  }

  // ========== ✅ 输出到屏幕（替代 Serial.println）==========
  tft.setTextColor(TEXT_COLOR);
  tft.printf("CPU: %s\n", cpuName);
  tft.printf("CPUTemp: %d°C  Load: %d%%  Freq: %dMHz\n", cpuTemp, cpuLoad, cpuFreq);
  tft.printf("GPU: %s\n", gpuName);
  tft.printf("GPUTemp: %d°C  Load: %d%%  Freq: %dMHz\n", gpuTemp, gpuLoad, gpuFreq);
  // tft.printf("VRAM: %dMB (%dMB used, %d%%)\n", memTotal, memUsed, memLoad);
  // tft.printf("RAM: %.1fG (%.1fG used, %.0f%%)\n", ramTotal, ramActive, ramLoad);
  // tft.printf("Fan: %s  RPM: %s\n", fan, rpm);
  // tft.printf("Power: %.1fW\n", power);
  // tft.printf("MemClk: %dMHz  ShaderClk: %dMHz\n", gpuMemClock, gpuShaderClock);
}

// 清屏
void clearScreen() {
  tft.fillScreen(BG_COLOR);
  tft.setCursor(0, 0);
}

// 显示错误信息
void showError(const char* msg) {
  clearScreen();
  tft.setTextColor(ERROR_COLOR);
  tft.println("ERROR:");
  tft.println(msg);
  delay(2000);
}

// 重置缓冲区
void resetBuffer() {
  bufferIndex = 0;
  inputBuffer[0] = '\0';
}