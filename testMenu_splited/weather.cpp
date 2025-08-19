#include "weather.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "menu.h"
#include "RotaryEncoder.h"

// -----------------------------
// 🔧 配置区
// -----------------------------
const char *WEATHER_API_URL =
    "https://restapi.amap.com/v3/weather/"
    "weatherInfo?city=120104&key=8a4fcc66268926914fff0c968b3c804c";
const char *NTP_SERVER = "ntp.aliyun.com";
const long GMT_OFFSET_SEC = 8 * 3600;
const int DAYLIGHT_OFFSET = 0;
const long WEATHER_INTERVAL_S = 3 * 3600;
const long TIME_SYNC_INTERVAL_S = 6 * 3600;

// -----------------------------
// 全局变量
// -----------------------------
struct tm timeinfo;
String weather_main = "--";
String weather_temp = "--";
String weather_hum = "--";
bool wifi_connected = false;
unsigned long last_weather_update = 0;
unsigned long last_time_update = 0;
int last_sec = -1;
#define WIFI_SSID "xiaomiao_hotspot"
#define WIFI_PASSWORD "xiaomiao123"
// -----------------------------
// WiFi 和时间同步函数
// -----------------------------
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

// -----------------------------
// 绘制静态元素
// -----------------------------
void drawWeatherStaticElements() {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TITLE_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Weather", DATA_X, DATA_Y);
  tft.drawString("Time:", DATA_X, DATA_Y + LINE_HEIGHT);
  tft.drawString("Weather:", DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.drawString("Temp:", DATA_X, DATA_Y + 3 * LINE_HEIGHT);
  tft.drawString("Humidity:", DATA_X, DATA_Y + 4 * LINE_HEIGHT);
}

// -----------------------------
// 绘制时间
// -----------------------------
void drawWeatherTime() {
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %a", &timeinfo);
  tft.fillRect(DATA_X, DATA_Y + LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(buf, DATA_X, DATA_Y + LINE_HEIGHT);
  sprintf(buf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.fillRect(DATA_X + 60, DATA_Y + LINE_HEIGHT, 60, LINE_HEIGHT, BG_COLOR);
  tft.drawString(buf, DATA_X + 60, DATA_Y + LINE_HEIGHT);
}

// -----------------------------
// 更新天气数据
// -----------------------------
void updateWeatherDisplay() {
  tft.fillRect(DATA_X, DATA_Y + 2 * LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.setTextColor(VALUE_COLOR, BG_COLOR);
  tft.drawString(weather_main, DATA_X, DATA_Y + 2 * LINE_HEIGHT);
  tft.fillRect(DATA_X, DATA_Y + 3 * LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.drawString(weather_temp + "°C", DATA_X, DATA_Y + 3 * LINE_HEIGHT);
  tft.fillRect(DATA_X, DATA_Y + 4 * LINE_HEIGHT, 120, LINE_HEIGHT, BG_COLOR);
  tft.drawString(weather_hum + "%", DATA_X, DATA_Y + 4 * LINE_HEIGHT);
}

// -----------------------------
// 显示错误信息
// -----------------------------
void showWeatherError(const char* msg) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(ERROR_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Error:", tft.width() / 2, tft.height() / 2 - LINE_HEIGHT);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  delay(2000);
  drawWeatherStaticElements();
}

// -----------------------------
// 天气初始化任务
// -----------------------------
void Weather_Init_Task(void *pvParameters) {
  drawWeatherStaticElements();
  if (wifi_connected || connectWiFi()) {
    if (syncNTPTime()) {
      Serial.println("✅ Time synchronized");
      last_time_update = time(nullptr);
    }
    if (getWeather()) {
      Serial.println("✅ Weather data retrieved");
      last_weather_update = time(nullptr);
    }
    disconnectWiFi();
  } else {
    showWeatherError("WiFi failed");
  }
  vTaskDelete(NULL);
}

// -----------------------------
// 天气显示任务
// -----------------------------
void Weather_Task(void *pvParameters) {
  for (;;) {
    getLocalTime(&timeinfo);
    if (timeinfo.tm_sec != last_sec) {
      drawWeatherTime();
      last_sec = timeinfo.tm_sec;
    }
    updateWeatherDisplay();
    if (time(nullptr) - last_time_update >= TIME_SYNC_INTERVAL_S || time(nullptr) - last_weather_update >= WEATHER_INTERVAL_S) {
      if (connectWiFi()) {
        if (syncNTPTime()) last_time_update = time(nullptr);
        if (getWeather()) last_weather_update = time(nullptr);
        disconnectWiFi();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// -----------------------------
// 天气菜单入口
// -----------------------------
void weatherMenu() {
  animateMenuTransition("WEATHER",true);
  xTaskCreatePinnedToCore(Weather_Init_Task, "Weather_Init", 8192, NULL, 2, NULL, 0);
  TaskHandle_t weatherTask = NULL;
  xTaskCreatePinnedToCore(Weather_Task, "Weather_Show", 8192, NULL, 1, &weatherTask, 0);
  while (1) {
    if (readButton()) {
      animateMenuTransition("WEATHER",false);
      if (weatherTask != NULL) {
        vTaskDelete(weatherTask);
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}