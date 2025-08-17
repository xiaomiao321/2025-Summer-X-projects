#include <Adafruit_AHTX0.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#include "img.h"
// INSERT NVIDIA LOGO DATA HERE
// Example: const uint16_t nvidia_logo[] PROGMEM = { /* RGB565 data */ };
// Use Image2cpp to convert a 40x40 PNG/JPG to a 16-bit RGB565 array

// INSERT INTEL LOGO DATA HERE
// Example: const uint16_t intel_logo[] PROGMEM = { /* RGB565 data */ };
// Use Image2cpp to convert a 40x40 PNG/JPG to a 16-bit RGB565 array

// -----------------------------
// 🔧 配置区
// -----------------------------
const char *WIFI_SSID = "xiaomiao_hotspot";
const char *WIFI_PASSWORD = "xiaomiao123";
const char *WEATHER_API_URL =
    "https://restapi.amap.com/v3/weather/"
    "weatherInfo?city=120104&key=8a4fcc66268926914fff0c968b3c804c";
const char *NTP_SERVER = "ntp.aliyun.com";
const long GMT_OFFSET_SEC = 8 * 3600;
const int DAYLIGHT_OFFSET = 0;

const long SENSOR_INTERVAL_MS = 5000;
const long WEATHER_INTERVAL_S = 3 * 3600;
const long TIME_SYNC_INTERVAL_S = 6 * 3600;

// TFT 布局（240x240 屏幕）
#define LOGO_X 10
#define LOGO_Y_TOP 10
#define LOGO_Y_BOTTOM 60
#define DATA_X 60
#define DATA_Y 10
#define LINE_HEIGHT 18
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

// -----------------------------
// 全局对象
// -----------------------------
Adafruit_AHTX0 aht;
TFT_eSPI tft;

// PC 数据结构体
struct PCData {
  char cpuName[64] = "未知";
  char gpuName[64] = "未知";
  char rpm[16] = "未知";
  int cpuTemp = 0;
  int cpuLoad = 0;
  int gpuTemp = 0;
  int gpuLoad = 0;
  float ramLoad = 0.0;
  bool valid = false;
} pcData;

// ESP32-C3 温度
float esp32c3_temp = 0.0f;

// -----------------------------
// 其他全局变量
// -----------------------------
struct tm timeinfo;
float aht_temp = 0.0f, aht_hum = 0.0f;
String weather_main = "--";
String weather_temp = "--";
String weather_hum = "--";

bool aht_ok = false;
bool wifi_connected = false;

int last_sec = -1, last_min = -1;
unsigned long last_weather_update = 0;
unsigned long last_time_update = 0;

#define BUFFER_SIZE 512
char inputBuffer[BUFFER_SIZE];
uint16_t bufferIndex = 0;
bool stringComplete = false;

// -----------------------------
// 🔒 互斥量
// -----------------------------
SemaphoreHandle_t xPCDataMutex = NULL;

// -----------------------------
// 初始化 TFT
// -----------------------------
void initTFT() {
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(100);
  digitalWrite(TFT_RST, HIGH);
  delay(350);

  tft.init();
  tft.setRotation(1); // 横屏 240x240
  tft.fillScreen(BG_COLOR);
  delay(100);
  tft.fillScreen(TFT_WHITE);
  delay(300);
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextSize(1);
  // tft.setFreeFont(&FreeSans9pt7b); // 确保在 TFT_eSPI 中启用此字体
  // tft.setTextDatum(MC_DATUM);
  tft.drawString("Boot...", tft.width() / 2, tft.height() / 2);
  delay(1000);
}

// -----------------------------
// 绘制静态元素
// -----------------------------
void drawStaticElements() {
  tft.fillScreen(BG_COLOR);

  // 绘制 NVIDIA 和 Intel 标志
  // 示例：tft.drawRGBBitmap(LOGO_X, LOGO_Y_TOP, nvidia_logo, 40, 40);
  // 示例：tft.drawRGBBitmap(LOGO_X, LOGO_Y_BOTTOM, intel_logo, 40, 40);
  // 在此处插入标志绘制代码
   tft.pushImage(LOGO_X, LOGO_Y_TOP, 40, 40, NVIDIAGEFORCE);
  tft.pushImage(LOGO_X, LOGO_Y_BOTTOM,  40, 40,intelcore);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("ESP32 monitor", DATA_X, DATA_Y);
  tft.drawString("time:", DATA_X, DATA_Y + LINE_HEIGHT);
  tft.drawString("AHT sensor:", DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.drawString("weather:", DATA_X, DATA_Y + 3 * LINE_HEIGHT);
  tft.drawString("PC data:", DATA_X, DATA_Y + 4 * LINE_HEIGHT);
  tft.drawString("CPU name:", DATA_X + 10, DATA_Y + 5 * LINE_HEIGHT);
  tft.drawString("CPU load:", DATA_X + 10, DATA_Y + 7 * LINE_HEIGHT);
  tft.drawString("GPU name:", DATA_X + 10, DATA_Y + 8 * LINE_HEIGHT);
  tft.drawString("GPU load:", DATA_X + 10, DATA_Y + 10 * LINE_HEIGHT);
  tft.drawString("RAM load:", DATA_X + 10, DATA_Y + 11 * LINE_HEIGHT);
  // tft.drawString("Fan speed:", DATA_X + 10, DATA_Y + 12 * LINE_HEIGHT);
  tft.drawString("ESP32 Temp:", DATA_X + 10, DATA_Y + 12 * LINE_HEIGHT);
}

// -----------------------------
// 绘制时间
// -----------------------------
void drawTime() {
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %a", &timeinfo);
  tft.fillRect(DATA_X, DATA_Y + LINE_HEIGHT - 12, 180, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(buf, DATA_X, DATA_Y + LINE_HEIGHT);

  sprintf(buf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.fillRect(DATA_X + 60, DATA_Y + LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.drawString(buf, DATA_X + 60, DATA_Y + LINE_HEIGHT);
}

// -----------------------------
// 更新传感器和天气
// -----------------------------
void updateSensorsAndWeather() {
  // A HellenicHT 传感器
  tft.fillRect(DATA_X, DATA_Y + 2 * LINE_HEIGHT, 180, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  String ahtStr = String(aht_temp, 1) + "°C  " + String(aht_hum, 1) + "%";
  tft.drawString(ahtStr, DATA_X, DATA_Y + 2 * LINE_HEIGHT);

  // 天气
  tft.fillRect(DATA_X, DATA_Y + 3 * LINE_HEIGHT, 180, LINE_HEIGHT, BG_COLOR);
  String weatherStr = weather_temp + "°C  " + weather_hum + "%";
  tft.drawString(weatherStr, DATA_X, DATA_Y + 3 * LINE_HEIGHT);
}

// -----------------------------
// 更新 PC 数据和 ESP32-C3 温度
// -----------------------------
void updatePCData() {
  if (xSemaphoreTake(xPCDataMutex, 10) != pdTRUE)
    return;

  tft.setTextColor(VALUE_COLOR, BG_COLOR);

  if (pcData.valid) {
    // CPU 名称
    tft.fillRect(DATA_X + 80, DATA_Y + 5 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.cpuName), DATA_X + 80, DATA_Y + 5 * LINE_HEIGHT);

    // CPU 温度
    tft.fillRect(DATA_X + 80, DATA_Y + 6 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.cpuTemp) + "°C", DATA_X + 80, DATA_Y + 6 * LINE_HEIGHT);

    // CPU 负载
    tft.fillRect(DATA_X + 80, DATA_Y + 7 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.cpuLoad) + "%", DATA_X + 80, DATA_Y + 7 * LINE_HEIGHT);

    // GPU 名称
    tft.fillRect(DATA_X + 80, DATA_Y + 8 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.gpuName), DATA_X + 80, DATA_Y + 8 * LINE_HEIGHT);

    // GPU 温度
    tft.fillRect(DATA_X + 80, DATA_Y + 9 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.gpuTemp) + "°C", DATA_X + 80, DATA_Y + 9 * LINE_HEIGHT);

    // GPU 负载
    tft.fillRect(DATA_X + 80, DATA_Y + 10 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.gpuLoad) + "%", DATA_X + 80, DATA_Y + 10 * LINE_HEIGHT);

    // 内存负载
    tft.fillRect(DATA_X + 80, DATA_Y + 11 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    tft.drawString(String(pcData.ramLoad, 1) + "%", DATA_X + 80, DATA_Y + 11 * LINE_HEIGHT);

    // 风扇转速
    // tft.fillRect(DATA_X + 80, DATA_Y + 12 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
    // tft.drawString(String(pcData.rpm), DATA_X + 80, DATA_Y + 12 * LINE_HEIGHT);
  } else {
    tft.fillRect(DATA_X + 80, DATA_Y + 5 * LINE_HEIGHT, 100, 8 * LINE_HEIGHT, BG_COLOR);
    tft.setTextColor(ERROR_COLOR, BG_COLOR);
    tft.drawString("无数据", DATA_X + 80, DATA_Y + 5 * LINE_HEIGHT);
  }

  // ESP32-C3 温度
  tft.fillRect(DATA_X + 80, DATA_Y + 13 * LINE_HEIGHT, 100, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(String(esp32c3_temp, 1) + "°C", DATA_X + 80, DATA_Y + 12 * LINE_HEIGHT);

  xSemaphoreGive(xPCDataMutex);
}

// -----------------------------
// 显示错误信息
// -----------------------------
void showError(const char* msg) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(ERROR_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("错误:", tft.width() / 2, tft.height() / 2 - LINE_HEIGHT);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  delay(2000);
  drawStaticElements();
}

// -----------------------------
// 重置缓冲区
// -----------------------------
void resetBuffer() {
  bufferIndex = 0;
  inputBuffer[0] = '\0';
}

// -----------------------------
// 解析 PC 数据
// -----------------------------
void parsePCData() {
  PCData newData;

  char *ptr = nullptr;

  // 解析 CPU 温度和负载
  ptr = strstr(inputBuffer, "C");
  if (ptr) {
    char *degree = strchr(ptr, 'c');
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      newData.cpuTemp = atoi(temp);
      newData.cpuLoad = atoi(load);
    }
  }

  // 解析 GPU 温度和负载
  ptr = strstr(inputBuffer, "G");
  if (ptr) {
    char *degree =strstr(ptr, "c");
    char *limit = strchr(ptr, '%');
    if (degree && limit && degree < limit) {
      char temp[8] = {0}, load[8] = {0};
      strncpy(temp, ptr + 1, degree - ptr - 1);
      strncpy(load, degree + 2, limit - degree - 2);
      newData.gpuTemp = atoi(temp);
      newData.gpuLoad = atoi(load);
    }
  }

  // 解析 RAM 负载
  ptr = strstr(inputBuffer, "RL");
  if (ptr) {
    ptr += 2;
    char *end = strchr(ptr, '|');
    if (end) {
      char load[8] = {0};
      strncpy(load, ptr, end - ptr);
      newData.ramLoad = atof(load);
    }
  }

  // 解析 RPM
  ptr = strstr(inputBuffer, "GRPM");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (end && end - ptr < sizeof(newData.rpm) - 1) {
      strncpy(newData.rpm, ptr, end - ptr);
      newData.rpm[end - ptr] = '\0';
    }
  }

  // 解析 CPU 名称
  ptr = strstr(inputBuffer, "CPU:");
  if (ptr) {
    ptr += 4;
    char *end = strstr(ptr, "GPU:");
    if (!end) end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(newData.cpuName) - 1) {
      strncpy(newData.cpuName, ptr, end - ptr);
      newData.cpuName[end - ptr] = '\0';
      for (int i = strlen(newData.cpuName) - 1; i >= 0 && newData.cpuName[i] == ' '; i--)
        newData.cpuName[i] = '\0';
    }
  }

  // 解析 GPU 名称
  ptr = strstr(inputBuffer, "GPU:");
  if (ptr) {
    ptr += 4;
    char *end = strchr(ptr, '|');
    if (!end) end = inputBuffer + strlen(inputBuffer);
    if (end - ptr < sizeof(newData.gpuName) - 1) {
      strncpy(newData.gpuName, ptr, end - ptr);
      newData.gpuName[end - ptr] = '\0';
      for (int i = strlen(newData.gpuName) - 1; i >= 0 && newData.gpuName[i] == ' '; i--)
        newData.gpuName[i] = '\0';
    }
  }

  // 标记数据有效性
  if (newData.cpuTemp > 0 || newData.gpuTemp > 0 || newData.ramLoad > 0) {
    newData.valid = true;
  }

  // 安全更新 pcData
  if (xSemaphoreTake(xPCDataMutex, portMAX_DELAY) == pdTRUE) {
    pcData = newData;
    xSemaphoreGive(xPCDataMutex);
  }
}

// -----------------------------
// 任务实现
// -----------------------------
void AHT_Init_Task(void *pvParameters) {
  Wire.begin(20, 21);
  Serial.println("初始化 AHT...");
  aht_ok = aht.begin();
  if (aht_ok)
    Serial.println("✅ AHT 传感器已找到！");
  else
    Serial.println("❌ 未找到 AHT 传感器！");
  vTaskDelete(NULL);
}

void AHT_Task(void *pvParameters) {
  for (;;) {
    if (aht_ok) {
      sensors_event_t hum, temp;
      if (aht.getEvent(&hum, &temp)) {
        aht_temp = temp.temperature;
        aht_hum = hum.relative_humidity;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
  }
}

void WIFI_Init_Task(void *pvParameters) {
  if (connectWiFi()) {
    if (syncNTPTime()) {
      Serial.println("✅ 时间已同步");
      last_time_update = time(nullptr);
    }
    if (getWeather()) {
      Serial.println("✅ 天气数据已获取");
      last_weather_update = time(nullptr);
    }
    disconnectWiFi();
  }
  vTaskDelete(NULL);
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long timeout = millis() + 10000;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout)
    delay(500);
  wifi_connected = (WiFi.status() == WL_CONNECTED);
  return wifi_connected;
}

void disconnectWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  wifi_connected = false;
}

bool syncNTPTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
  return getLocalTime(&timeinfo, 10000);
}

bool getWeather() {
  HTTPClient http;
  http.begin(WEATHER_API_URL);
  int code = http.GET();
  bool success = false;

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    JsonArray lives = doc["lives"];
    if (lives.size() > 0) {
      weather_main = lives[0]["weather"].as<String>();
      weather_temp = lives[0]["temperature"].as<String>();
      weather_hum = lives[0]["humidity"].as<String>();
      success = true;
    }
  }
  http.end();
  return success;
}

void TFT_Init_Task(void *pvParameters) {
  initTFT();
  drawStaticElements();
  vTaskDelete(NULL);
}

void TFT_Task(void *pvParameters) {
  for (;;) {
    getLocalTime(&timeinfo);

    if (timeinfo.tm_sec != last_sec) {
      drawTime();
      last_sec = timeinfo.tm_sec;
    }

    if (timeinfo.tm_min != last_min || millis() % 5000 < 100) {
      updateSensorsAndWeather();
      esp32c3_temp = temperatureRead(); // 获取 ESP32-C3 温度
      updatePCData();
      last_min = timeinfo.tm_min;
    }

    // 定期同步时间
    if (time(nullptr) - last_time_update >= TIME_SYNC_INTERVAL_S) {
      xTaskCreatePinnedToCore(WIFI_Init_Task, "WiFi_Init", 8192, NULL, 2, NULL, 0);
      last_time_update = time(nullptr);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void SERIAL_Task(void *pvParameters) {
  for (;;) {
    if (Serial.available()) {
      char inChar = (char)Serial.read();

      if (bufferIndex < BUFFER_SIZE - 1) {
        inputBuffer[bufferIndex++] = inChar;
        inputBuffer[bufferIndex] = '\0';
      } else {
        //showError("缓冲区满！");
        resetBuffer();
      }

      // 检测换行符
      if (inChar == '\n') {
        stringComplete = true;
      }
    }

    // 处理完整数据
    if (stringComplete) {
      parsePCData();
      stringComplete = false;
      resetBuffer();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// -----------------------------
// setup() & loop()
// -----------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  // 创建互斥量
  xPCDataMutex = xSemaphoreCreateMutex();

  // 启动初始化任务
  xTaskCreatePinnedToCore(AHT_Init_Task, "AHT_Init", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TFT_Init_Task, "TFT_Init", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(WIFI_Init_Task, "WiFi_Init", 8192, NULL, 2, NULL, 0);

  // 启动运行任务
  xTaskCreatePinnedToCore(AHT_Task, "AHT_Read", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TFT_Task, "TFT_Show", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(SERIAL_Task, "Serial_Rx", 2048, NULL, 1, NULL, 0);
}

void loop() { delay(1000); }