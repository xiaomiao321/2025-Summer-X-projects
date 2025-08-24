#include <TFT_eSPI.h>      // TFT_eSPI 库
#include <esp_system.h>    // ESP32 系统库，用于 RTC
#include <WiFi.h>          // Wi-Fi 库
#include <time.h>          // 时间库
#include <HTTPClient.h>    // HTTP 客户端
#include <ArduinoJson.h>   // JSON 解析
#include <OneWire.h>       // OneWire 库
#include <DallasTemperature.h> // DS18B20 库
#include "RotaryEncoder.h" // 自定义旋转编码器库

// Wi-Fi 和 API 配置
#define WIFI_SSID "xiaomiao"
#define WIFI_PASSWORD "xiaomiao123"
const char* ntpServer = "ntp.aliyun.com";
const char* weatherApiUrl = "https://restapi.amap.com/v3/weather/weatherInfo?city=120104&key=8a4fcc66268926914fff0c968b3c804c";
const long gmtOffset_sec = 8 * 3600;    // 中国时区 UTC+8
const int daylightOffset_sec = 0;       // 无夏令时
const unsigned long ntpInterval = 6 * 3600 * 1000; // 每6小时同步时间
const unsigned long weatherInterval = 3 * 3600 * 1000; // 每3小时更新天气
const int wifiMaxRetries = 5; // Wi-Fi 重试次数
const unsigned long wifiRetryDelay = 10000; // 重试间隔10秒

// DS18B20 配置
#define ONE_WIRE_BUS 5 // DS18B20 数据引脚
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

TFT_eSPI tft = TFT_eSPI(); // 创建 TFT_eSPI 对象

// 全局变量
struct tm timeinfo;
String weather_main = "--";
String weather_temp = "--";
String weather_hum = "--";
float sensor_temp = -127.0; // DS18B20 温度，初始值表示无效
bool wifi_connected = false;
unsigned long lastWeatherUpdate = 0;
unsigned long lastNTPSync = 0;

// 旋转编码器相关
bool isAdjusting = false;  // 是否处于调整模式
int adjustField = 0;       // 当前调整字段（0:年, 1:月, 2:日, 3:时, 4:分, 5:秒）
unsigned long lastButtonPress = 0; // 上次按钮按下时间
const unsigned long longPressDuration = 2000; // 长按 2 秒
const unsigned long animationDuration = 1000; // 圆圈动画 1 秒
bool animationActive = false; // 是否显示圆圈动画
unsigned long animationStart = 0; // 动画开始时间

// 显示参数
#define DATA_X 10
#define DATA_Y 10
#define LINE_HEIGHT 20
#define TIME_HEIGHT 32 // 放大时间字体的高度
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_WHITE
#define VALUE_COLOR TFT_GREEN
#define ERROR_COLOR TFT_RED

void setup() {
  Serial.begin(115200); // 初始化串口
  initRotaryEncoder();  // 初始化旋转编码器
  sensors.begin();      // 初始化 DS18B20

  // 初始化 TFT 显示屏 (ST7789)
  tft.init();
  tft.setRotation(1); // 设置屏幕方向（0-3，根据需要调整）
  tft.fillScreen(BG_COLOR); // 清屏为黑色
  tft.setTextColor(TITLE_COLOR, BG_COLOR); // 设置文字颜色和背景
  tft.setTextDatum(TL_DATUM); // 左上角对齐

  // 绘制静态元素
  drawStaticElements();

  // 初始化 ESP32-C3 内部 RTC
  struct tm timeinfo;
  timeinfo.tm_year = 2025 - 1900; // 年份从 1900 开始
  timeinfo.tm_mon = 8 - 1;        // 月份从 0 开始
  timeinfo.tm_mday = 20;          // 日
  timeinfo.tm_hour = 16;          // 时
  timeinfo.tm_min = 55;           // 分
  timeinfo.tm_sec = 0;            // 秒
  time_t t = mktime(&timeinfo);
  struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
  settimeofday(&tv, NULL); // 设置初始时间

  // 尝试连接 Wi-Fi 并同步时间和天气
  if (connectWiFi()) {
    syncNTP();
    getWeather();
    disconnectWiFi();
  } else {
    showError("WiFi failed");
  }
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long nowMillis = millis();

  // 获取当前时间
  time_t now;
  time(&now);
  localtime_r(&now, &timeinfo);

  // 处理旋转编码器
  handleEncoder(&timeinfo);

  // 每秒更新显示
  if (nowMillis - lastUpdate >= 1000) {
    // 读取 DS18B20 温度
    sensors.requestTemperatures();
    sensor_temp = sensors.getTempCByIndex(0); // 获取第一个传感器温度
    updateDisplay(&timeinfo);
    lastUpdate = nowMillis;
  }

  // 每隔 ntpInterval 同步 NTP
  if (nowMillis - lastNTPSync >= ntpInterval) {
    if (connectWiFi()) {
      syncNTP();
      disconnectWiFi();
    }
    lastNTPSync = nowMillis;
  }

  // 每隔 weatherInterval 更新天气
  if (nowMillis - lastWeatherUpdate >= weatherInterval) {
    if (connectWiFi()) {
      getWeather();
      disconnectWiFi();
    }
    lastWeatherUpdate = nowMillis;
  }
}

// 连接 Wi-Fi（支持多次重试）
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  int retries = 0;
  while (retries < wifiMaxRetries) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("连接 Wi-Fi... 尝试 ");
    Serial.println(retries + 1);
    unsigned long timeout = millis() + wifiRetryDelay;
    while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      Serial.println("已连接！");
      return true;
    }
    WiFi.disconnect();
    retries++;
    delay(1000); // 等待1秒后重试
  }
  wifi_connected = false;
  Serial.println("Wi-Fi 连接失败！");
  return false;
}

// 断开 Wi-Fi
void disconnectWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  wifi_connected = false;
  Serial.println("Wi-Fi 已断开");
}

// 同步 NTP 时间
void syncNTP() {
  if (wifi_connected) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (getLocalTime(&timeinfo, 10000)) {
      Serial.printf("NTP 同步时间: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      Serial.println("NTP 同步失败");
    }
  }
}

// 获取天气数据
bool getWeather() {
  if (!wifi_connected) return false;
  HTTPClient http;
  http.begin(weatherApiUrl);
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
      Serial.println("天气数据更新成功");
    }
  } else {
    Serial.println("天气数据获取失败");
  }
  http.end();
  return success;
}

// 绘制静态元素
void drawStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.drawString("Time:", DATA_X, DATA_Y);
  tft.drawString("Weather:", DATA_X, DATA_Y + TIME_HEIGHT + LINE_HEIGHT);
  tft.drawString("Temp:", DATA_X, DATA_Y + TIME_HEIGHT + 2 * LINE_HEIGHT);
  tft.drawString("Humidity:", DATA_X, DATA_Y + TIME_HEIGHT + 3 * LINE_HEIGHT);
  tft.drawString("Sensor:", DATA_X, DATA_Y + TIME_HEIGHT + 4 * LINE_HEIGHT);
  tft.drawString("WiFi:", DATA_X, DATA_Y + TIME_HEIGHT + 5 * LINE_HEIGHT);
}

// 显示错误信息
void showError(const char* msg) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(ERROR_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString("Error:", tft.width() / 2, tft.height() / 2 - LINE_HEIGHT);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  delay(2000);
  drawStaticElements();
}

// 处理旋转编码器输入
void handleEncoder(struct tm *timeinfo) {
  static bool lastButtonState = false;
  int buttonState = readButton();
  int encoderDelta = readEncoder();
  unsigned long currentTime = millis();

  // 检测长按（显示圆圈动画并进入调整模式）
  if (buttonState && !lastButtonState) {
    lastButtonPress = currentTime;
    animationActive = true;
    animationStart = currentTime;
  }

  if (animationActive && buttonState) {
    // 显示圆圈动画
    float progress = (float)(currentTime - animationStart) / animationDuration;
    if (progress <= 1.0) {
      tft.fillScreen(BG_COLOR);
      int radius = 20 + progress * 20; // 圆圈半径从 20 到 40
      tft.drawCircle(tft.width() / 2, tft.height() / 2, radius, TITLE_COLOR);
    } else {
      // 动画完成，进入调整模式
      animationActive = false;
      isAdjusting = true;
      tft.fillScreen(BG_COLOR); // 清屏以进入调整显示
      drawStaticElements();
    }
  }

  // 检测长按释放（退出调整模式）
  if (!buttonState && lastButtonState && isAdjusting && (currentTime - lastButtonPress >= longPressDuration)) {
    isAdjusting = false;
    tft.fillScreen(BG_COLOR); // 清屏以恢复正常显示
    drawStaticElements();
    // 更新 RTC 时间
    time_t t = mktime(timeinfo);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    Serial.printf("时间已更新: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  }

  // 检测单击（切换调整字段）
  if (!buttonState && lastButtonState && (currentTime - lastButtonPress < longPressDuration)) {
    if (isAdjusting) {
      adjustField = (adjustField + 1) % 6; // 切换字段（年、月、日、时、分、秒）
      Serial.printf("切换到字段 %d\n", adjustField);
    }
  }

  // 处理旋转编码器调整
  if (isAdjusting && encoderDelta != 0) {
    switch (adjustField) {
      case 0: timeinfo->tm_year += encoderDelta; break; // 年
      case 1: timeinfo->tm_mon += encoderDelta; break;  // 月
      case 2: timeinfo->tm_mday += encoderDelta; break; // 日
      case 3: timeinfo->tm_hour += encoderDelta; break; // 时
      case 4: timeinfo->tm_min += encoderDelta; break;  // 分
      case 5: timeinfo->tm_sec += encoderDelta; break;  // 秒
    }
    // 限制字段范围（考虑闰年和每月天数）
    if (timeinfo->tm_year < 0) timeinfo->tm_year = 0;
    if (timeinfo->tm_mon < 0) timeinfo->tm_mon = 0;
    if (timeinfo->tm_mon > 11) timeinfo->tm_mon = 11;
    int daysInMonth = 31;
    if (timeinfo->tm_mon == 3 || timeinfo->tm_mon == 5 || timeinfo->tm_mon == 8 || timeinfo->tm_mon == 10) {
      daysInMonth = 30;
    } else if (timeinfo->tm_mon == 1) {
      bool isLeap = (timeinfo->tm_year % 4 == 0 && timeinfo->tm_year % 100 != 0) || (timeinfo->tm_year % 400 == 0);
      daysInMonth = isLeap ? 29 : 28;
    }
    if (timeinfo->tm_mday < 1) timeinfo->tm_mday = 1;
    if (timeinfo->tm_mday > daysInMonth) timeinfo->tm_mday = daysInMonth;
    if (timeinfo->tm_hour < 0) timeinfo->tm_hour = 0;
    if (timeinfo->tm_hour > 23) timeinfo->tm_hour = 23;
    if (timeinfo->tm_min < 0) timeinfo->tm_min = 0;
    if (timeinfo->tm_min > 59) timeinfo->tm_min = 59;
    if (timeinfo->tm_sec < 0) timeinfo->tm_sec = 0;
    if (timeinfo->tm_sec > 59) timeinfo->tm_sec = 59;
    // 实时更新 RTC
    time_t t = mktime(timeinfo);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    updateDisplay(timeinfo); // 立即刷新显示
  }

  lastButtonState = buttonState;
}

// 更新屏幕显示
void updateDisplay(struct tm *timeinfo) {
  // 更新时间（大字体）
  char timeStr[20];
  snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  tft.fillRect(DATA_X + 60, DATA_Y, 200, TIME_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.setTextSize(2); // 放大时间字体
  tft.drawString(timeStr, DATA_X + 60, DATA_Y);
  tft.setTextSize(1); // 其他信息用小字体

  // 更新天气
  tft.fillRect(DATA_X + 60, DATA_Y + TIME_HEIGHT + LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(weather_main, DATA_X + 60, DATA_Y + TIME_HEIGHT + LINE_HEIGHT);
  tft.fillRect(DATA_X + 60, DATA_Y + TIME_HEIGHT + 2 * LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.drawString(weather_temp + "°C", DATA_X + 60, DATA_Y + TIME_HEIGHT + 2 * LINE_HEIGHT);
  tft.fillRect(DATA_X + 60, DATA_Y + TIME_HEIGHT + 3 * LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.drawString(weather_hum + "%", DATA_X + 60, DATA_Y + TIME_HEIGHT + 3 * LINE_HEIGHT);

  // 更新传感器温度
  tft.fillRect(DATA_X + 60, DATA_Y + TIME_HEIGHT + 4 * LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  if (sensor_temp != -127.0) { // 检查温度有效性
    char tempStr[10];
    snprintf(tempStr, sizeof(tempStr), "%.1f°C", sensor_temp);
    tft.drawString(tempStr, DATA_X + 60, DATA_Y + TIME_HEIGHT + 4 * LINE_HEIGHT);
  } else {
    tft.drawString("N/A", DATA_X + 60, DATA_Y + TIME_HEIGHT + 4 * LINE_HEIGHT);
  }

  // 更新 Wi-Fi 状态
  tft.fillRect(DATA_X + 60, DATA_Y + TIME_HEIGHT + 5 * LINE_HEIGHT, 60, LINE_HEIGHT, BG_COLOR);
  tft.drawString(wifi_connected ? "ON" : "OFF", DATA_X + 60, DATA_Y + TIME_HEIGHT + 5 * LINE_HEIGHT);

  // 如果在调整模式，绘制选中字段的白色框
  if (isAdjusting) {
    int x, w;
    switch (adjustField) {
      case 0: x = DATA_X + 60; w = 80; break; // 年 (4 字符，放大后约 48 像素宽)
      case 1: x = DATA_X + 140; w = 40; break; // 月 (2 字符)
      case 2: x = DATA_X + 180; w = 40; break; // 日 (2 字符)
      case 3: x = DATA_X + 220; w = 40; break; // 时 (2 字符)
      case 4: x = DATA_X + 260; w = 40; break; // 分 (2 字符)
      case 5: x = DATA_X + 300; w = 40; break; // 秒 (2 字符)
    }
    tft.drawRect(x, DATA_Y, w, TIME_HEIGHT, TITLE_COLOR);
  }
}