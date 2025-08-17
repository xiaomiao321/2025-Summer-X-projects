#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// -----------------------------
// 配置区
// -----------------------------
const char* ssid     = "xiaomiao_hotspot";
const char* password = "xiaomiao123";

// 高德天气 API
#define API_URL "https://restapi.amap.com/v3/weather/weatherInfo?city=120104&key=8a4fcc66268926914fff0c968b3c804c"

// 时间设置（NTP）
const char* ntpServer = "ntp.aliyun.com";
const long  gmtOffset_sec = 8 * 3600;         // 北京时间 UTC+8
const int   daylightOffset_sec = 0;

// 定时任务间隔
const long  TIME_UPDATE_INTERVAL = 6 * 3600;  // 每 6 小时同步时间
const long  WEATHER_UPDATE_INTERVAL = 3 * 3600; // 每 3 小时更新天气

// 显示刷新间隔
const int UPDATE_DISPLAY_INTERVAL = 1000;     // 每秒刷新时间

// 存储天气数据
String currentWeather = "--";
String currentTemp = "--";
String currentHumidity = "--";

// 时间结构体
struct tm timeinfo;

// 上次更新时间戳
unsigned long lastTimeUpdate = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastDisplayUpdate = 0;
int lastSecond = -1;
int lastMinute = -1;

#define BAR_LENGTH      200  // 进度条长度
#define BAR_HEIGHT      10   // 进度条高度
#define BAR_MARGIN      8    // 进度条间距
#define DAY_SECOND      86400 // 24*3600

// 计算居中 X 起始位置
#define BAR_X           ((tft.width() - BAR_LENGTH) / 2)

// Y 位置
#define BAR_DAY_Y       160
#define BAR_HOUR_Y      (BAR_DAY_Y + BAR_HEIGHT + BAR_MARGIN)
#define BAR_MINUTE_Y    (BAR_HOUR_Y + BAR_HEIGHT + BAR_MARGIN)

char percentStr[12]; // 全局缓冲区

// 初始化 TFT 屏幕
void initTFT() {
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(100);
  digitalWrite(TFT_RST, HIGH);
  delay(350);

  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);
  delay(100);
  tft.fillScreen(TFT_WHITE);
  delay(300);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(2);
  tft.drawString("Starting...", tft.width()/2, tft.height()/2);
}

//  只刷新时间数字（避免全屏闪烁）
void updateTimeDisplay() {
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // 清除旧时间区域
  tft.fillRect(tft.width()/2 - 80, 40, 160, 50, TFT_BLACK);
  
  tft.setTextFont(4);
  tft.setTextColor(TFT_CYAN);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(timeStr, tft.width()/2, 60);
}

// -----------------------------
// 📅 绘制静态内容（只在启动时画一次）
// -----------------------------
void drawStaticElements() {
  // 日期
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  char dateStr[40];
  strftime(dateStr, sizeof(dateStr), "%Y.%m.%d. %A", &timeinfo);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(dateStr, tft.width()/2, 100);

  // 天气标题
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("Real-time Weather", tft.width()/2, 130);

  // 天气信息（初始）
  // tft.setTextColor(TFT_WHITE);
  // tft.drawString("Weather: --", tft.width()/2, 160);
  // tft.drawString("Temperature: -- °C", tft.width()/2, 180);
  // tft.drawString("Humidity: -- %", tft.width()/2, 200);
}

// -----------------------------
// 📊 绘制进度条（每分钟刷新一次）
// -----------------------------
void drawSingleBar(int y, uint32_t current, uint32_t total, const char* label) {
  // 边框
  tft.drawRect(BAR_X, y, BAR_LENGTH, BAR_HEIGHT, TFT_DARKGREY);
  
  // 进度填充
  int progress = (current * BAR_LENGTH) / total;
  if (progress > 0) {
    uint16_t color = (progress >= BAR_LENGTH) ? TFT_GREEN : TFT_CYAN;
    tft.fillRect(BAR_X + 1, y + 1, progress - 1, BAR_HEIGHT - 1, color);
  }

  // 百分比文字
  // uint8_t percent = (uint8_t)((float)current / total * 100);
  // sprintf(percentStr, "%s:%u%%", label, percent);
  // tft.setTextFont(1);
  // tft.setTextColor(TFT_WHITE);
  // tft.setTextDatum(TR_DATUM); // 右对齐
  // tft.drawString(percentStr, BAR_X + BAR_LENGTH + 30, y + BAR_HEIGHT - 1);
}

void drawProgressBar() {
  uint32_t secOfDay = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  uint16_t secOfHour = timeinfo.tm_min * 60 + timeinfo.tm_sec;

  drawSingleBar(BAR_DAY_Y,     secOfDay,     DAY_SECOND,   "Day");
  drawSingleBar(BAR_HOUR_Y,    secOfHour,    3600,         "Hour");
  drawSingleBar(BAR_MINUTE_Y,  timeinfo.tm_sec, 60,        "Min");
}

// -----------------------------
// 🌤️ 更新天气信息
// -----------------------------
String getValue(String data, String key, String end) {
  int start = data.indexOf(key);
  if (start == -1) return "N/A";
  start += key.length();
  int endIndex = data.indexOf(end, start);
  if (endIndex == -1) return "N/A";
  return data.substring(start, endIndex);
}

bool getWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(API_URL);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    int startIndex = payload.indexOf("\"lives\":[{");
    if (startIndex == -1) {
      http.end();
      return false;
    }
    startIndex += 9;
    int endIndex = payload.indexOf("}]", startIndex);
    if (endIndex == -1) {
      http.end();
      return false;
    }
    String liveData = payload.substring(startIndex, endIndex);

    currentTemp = getValue(liveData, "\"temperature\":\"", "\"");
    currentHumidity = getValue(liveData, "\"humidity\":\"", "\"");
    currentWeather = getValue(liveData, "\"weather\":\"", "\"");

    Serial.println(liveData);
    http.end();
    return true;
  }
  http.end();
  return false;
}

// -----------------------------
// 🕰️ 同步 NTP 时间
// -----------------------------
bool syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  return getLocalTime(&timeinfo, 10000);
}

// -----------------------------
// 📶 Wi-Fi 控制
// -----------------------------
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int timeout = 15;
  while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
    delay(1000);
  }
  return WiFi.status() == WL_CONNECTED;
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// -----------------------------
// setup()
// -----------------------------
void setup() {
  Serial.begin(115200);
  initTFT();

  // 首次同步时间与天气
  if (connectWiFi()) {
    if (syncTime()) {
      Serial.println("初始时间同步成功");
      getWeather();
      Serial.println("初始天气获取成功");
    }
    disconnectWiFi();
  } else {
    getLocalTime(&timeinfo);
    Serial.println("WiFi连接失败，使用本地时间");
  }

  // 绘制静态内容
  tft.fillScreen(TFT_BLACK);
  drawStaticElements();
  drawProgressBar(); // 初始绘制一次进度条

  // 初始化时间戳
  lastSecond = timeinfo.tm_sec;
  lastMinute = timeinfo.tm_min;
  lastTimeUpdate = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  lastWeatherUpdate = lastTimeUpdate;
  lastDisplayUpdate = millis();
}

// -----------------------------
// loop()
// -----------------------------
void loop() {
  unsigned long currentMillis = millis();
  getLocalTime(&timeinfo); // 每次 loop 都获取最新时间
  unsigned long currentSecs = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;

  // 每秒刷新时间
  if (timeinfo.tm_sec != lastSecond) {
    updateTimeDisplay();
    drawProgressBar();
    lastSecond = timeinfo.tm_sec;
  }

  // 每 6 小时同步时间
  if (currentSecs - lastTimeUpdate >= TIME_UPDATE_INTERVAL) {
    if (connectWiFi() && syncTime()) {
      lastTimeUpdate = currentSecs;
      Serial.println("时间已更新");
    }
    disconnectWiFi();
  }

  // 每 3 小时更新天气
  if (currentSecs - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
    if (connectWiFi() && getWeather()) {
      lastWeatherUpdate = currentSecs;
      Serial.println("天气已更新");
    }
    disconnectWiFi();
  }

  delay(10); // 降低 CPU 占用
}