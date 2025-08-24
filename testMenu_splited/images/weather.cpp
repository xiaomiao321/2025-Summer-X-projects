// weather.cpp 修改内容
#include "weather.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "menu.h"
#include "RotaryEncoder.h"

// -----------------------------
// 全局变量
// -----------------------------
String weather_main = "--";
String weather_temp = "--";
String weather_hum = "--";
bool wifi_connected = false;
unsigned long last_weather_update = 0;
unsigned long last_drink_reminder = 0;
bool show_reminder = false;
unsigned long reminder_start_time = 0;
int last_sec = -1;
TimeAdjustMode timeAdjustMode = MODE_NORMAL;

// RTC时间结构
struct RTC_Time {
  int hour;
  int minute;
  int second;
};

RTC_Time rtc_time = {12, 0, 0}; // 默认时间

// -----------------------------
// WiFi 状态打印函数
// -----------------------------
void printWiFiStatus() {
  switch(WiFi.status()) {
    case WL_NO_SHIELD: Serial.println("WL_NO_SHIELD"); break;
    case WL_IDLE_STATUS: Serial.println("WL_IDLE_STATUS"); break;
    case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL"); break;
    case WL_SCAN_COMPLETED: Serial.println("WL_SCAN_COMPLETED"); break;
    case WL_CONNECTED: Serial.println("WL_CONNECTED"); break;
    case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED"); break;
    case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
    case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
    default: Serial.println("Unknown status"); break;
  }
}

// -----------------------------
// WiFi 事件处理
// -----------------------------
void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("Obtained IP: ");
      Serial.println(WiFi.localIP());
      wifi_connected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi disconnected");
      wifi_connected = false;
      break;
    default:
      break;
  }
}

// -----------------------------
// WiFi 连接函数（带重试机制）
// -----------------------------
bool connectWiFi() {
  Serial.println("Attempting to connect to: " + String(WIFI_SSID));
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);
  
  // 注册WiFi事件回调
  WiFi.onEvent(WiFiEvent);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  Serial.print("Connecting");
  int retryCount = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retryCount++;
    
    // 每10秒尝试重新连接一次
    if (retryCount % 20 == 0) {
      Serial.println("\nRetrying connection...");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    
    if (millis() - startTime > 30000) {  // 30秒超时
      Serial.println("\n❌ Connection timeout after 30 seconds");
      printWiFiStatus();
      return false;
    }
  }
  
  Serial.println("\n✅ Connected successfully!");
  Serial.println("IP address: " + WiFi.localIP().toString());
  wifi_connected = true;
  return true;
}

// -----------------------------
// 断开WiFi连接
// -----------------------------
void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifi_connected = false;
  Serial.println("WiFi disconnected");
}

// -----------------------------
// 更新RTC时间
// -----------------------------
void updateRTCtime() {
  rtc_time.second++;
  if (rtc_time.second >= 60) {
    rtc_time.second = 0;
    rtc_time.minute++;
    if (rtc_time.minute >= 60) {
      rtc_time.minute = 0;
      rtc_time.hour++;
      if (rtc_time.hour >= 24) {
        rtc_time.hour = 0;
      }
    }
  }
}

// -----------------------------
// 获取天气数据
// -----------------------------
bool getWeather() {
  Serial.println("Fetching weather data...");
  
  String apiUrl = String("https://restapi.amap.com/v3/weather/weatherInfo?city=") + 
                  String(WEATHER_API_CITY) + "&key=" + String(WEATHER_API_KEY);

  HTTPClient http;
  http.begin(apiUrl);
  http.setTimeout(10000);  // 10秒超时
  
  int code = http.GET();
  bool success = false;
  
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Weather API response received");
    
    DynamicJsonDocument doc(2048);  // 增加缓冲区大小
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      JsonArray lives = doc["lives"];
      if (lives.size() > 0) {
        weather_main = lives[0]["weather"].as<String>();
        weather_temp = lives[0]["temperature"].as<String>();
        weather_hum = lives[0]["humidity"].as<String>();
        
        Serial.println("✅ Weather data parsed successfully:");
        Serial.println("   Main: " + weather_main);
        Serial.println("   Temp: " + weather_temp + "°C");
        Serial.println("   Hum: " + weather_hum + "%");
        
        success = true;
      } else {
        Serial.println("❌ No weather data in response");
      }
    } else {
      Serial.println("❌ JSON parsing failed: " + String(error.c_str()));
    }
  } else {
    Serial.println("❌ HTTP error: " + String(code));
    Serial.println("Error: " + http.errorToString(code));
  }
  
  http.end();
  return success;
}

// -----------------------------
// 绘制进度条
// -----------------------------
void drawProgressBar(int x, int y, int width, int height, int progress, uint16_t color) {
  // 绘制边框
  tft.drawRect(x, y, width, height, TFT_WHITE);
  // 绘制进度
  int fillWidth = (width - 2) * progress / 100;
  tft.fillRect(x + 1, y + 1, fillWidth, height - 2, color);
}

// -----------------------------
// 绘制喝水提醒
// -----------------------------
void drawDrinkReminder() {
  if (show_reminder) {
    tft.fillRect(0, 0, 128, 20, COLOR_REMINDER_BG);
    tft.setTextColor(TFT_BLACK, COLOR_REMINDER_BG);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("💧 Time to Drink Water!", 64, 10);
    
    // 检查提醒是否结束
    if (millis() - reminder_start_time > DRINK_REMINDER_DURATION_S * 1000) {
      show_reminder = false;
    }
  }
}

// -----------------------------
// 绘制静态元素
// -----------------------------
void drawWeatherStaticElements() {
  tft.fillScreen(COLOR_BG_MAIN);
  
  // 标题
  tft.setTextColor(COLOR_TEXT_TITLE, COLOR_BG_MAIN);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Home Screen", WEATHER_DATA_X, 5);
  
  // 时间标签
  tft.drawString("Time:", WEATHER_DATA_X, WEATHER_DATA_Y);
  tft.drawString("Weather:", WEATHER_DATA_X, WEATHER_DATA_Y + WEATHER_LINE_HEIGHT);
  tft.drawString("Temp:", WEATHER_DATA_X, WEATHER_DATA_Y + 2 * WEATHER_LINE_HEIGHT);
  tft.drawString("Humidity:", WEATHER_DATA_X, WEATHER_DATA_Y + 3 * WEATHER_LINE_HEIGHT);
  
  // 进度条标签
  tft.drawString("Next Drink:", WEATHER_DATA_X, WEATHER_DATA_Y + 4 * WEATHER_LINE_HEIGHT + 5);
}

// -----------------------------
// 绘制时间（支持调整模式）
// -----------------------------
void drawWeatherTime() {
  char buf[32];
  // Display current date (fixed for internal RTC)
  tft.fillRect(WEATHER_DATA_X, WEATHER_DATA_Y, 120, WEATHER_LINE_HEIGHT, COLOR_BG_MAIN);
  tft.setTextColor(COLOR_TEXT_VALUE, COLOR_BG_MAIN);
  tft.drawString("2024-01-01 Mon", WEATHER_DATA_X, WEATHER_DATA_Y);
  
  // 绘制时间（高亮当前调整的项目）
  sprintf(buf, "%02d:%02d:%02d", rtc_time.hour, rtc_time.minute, rtc_time.second);
  
  if (timeAdjustMode == MODE_ADJUST_HOUR) {
    // 高亮小时
    tft.setTextColor(COLOR_TEXT_ERROR, COLOR_BG_MAIN);
    tft.drawString(buf, WEATHER_DATA_X + 60, WEATHER_DATA_Y);
    tft.setTextColor(COLOR_TEXT_VALUE, COLOR_BG_MAIN);
    tft.drawString(buf + 3, WEATHER_DATA_X + 60 + tft.textWidth("00"), WEATHER_DATA_Y); // 分钟和秒
  } else if (timeAdjustMode == MODE_ADJUST_MINUTE) {
    // 高亮分钟
    tft.setTextColor(COLOR_TEXT_VALUE, COLOR_BG_MAIN);
    tft.drawString("00:", WEATHER_DATA_X + 60, WEATHER_DATA_Y);
    tft.setTextColor(COLOR_TEXT_ERROR, COLOR_BG_MAIN);
    tft.drawString(buf + 3, WEATHER_DATA_X + 60 + tft.textWidth("00:"), WEATHER_DATA_Y);
    tft.setTextColor(COLOR_TEXT_VALUE, COLOR_BG_MAIN);
    tft.drawString(buf + 6, WEATHER_DATA_X + 60 + tft.textWidth("00:00"), WEATHER_DATA_Y); // 秒
  } else {
    // 正常显示
    tft.setTextColor(COLOR_TEXT_VALUE, COLOR_BG_MAIN);
    tft.drawString(buf, WEATHER_DATA_X + 60, WEATHER_DATA_Y);
  }
}

// -----------------------------
// 更新天气数据
// -----------------------------
void updateWeatherDisplay() {
  tft.fillRect(WEATHER_DATA_X, WEATHER_DATA_Y + WEATHER_LINE_HEIGHT, 120, 3 * WEATHER_LINE_HEIGHT, COLOR_BG_MAIN);
  tft.setTextColor(COLOR_TEXT_VALUE, COLOR_BG_MAIN);
  tft.drawString(weather_main, WEATHER_DATA_X, WEATHER_DATA_Y + WEATHER_LINE_HEIGHT);
  tft.drawString(weather_temp + "°C", WEATHER_DATA_X, WEATHER_DATA_Y + 2 * WEATHER_LINE_HEIGHT);
  tft.drawString(weather_hum + "%", WEATHER_DATA_X, WEATHER_DATA_Y + 3 * WEATHER_LINE_HEIGHT);
}

// -----------------------------
// 更新喝水进度条
// -----------------------------
void updateDrinkProgress() {
  unsigned long currentTime = time(nullptr);
  int progress = 100 - ((currentTime - last_drink_reminder) * 100 / DRINK_REMINDER_INTERVAL_S);
  progress = constrain(progress, 0, 100);
  
  // 绘制进度条
  drawProgressBar(WEATHER_DATA_X, WEATHER_DATA_Y + 4 * WEATHER_LINE_HEIGHT + 20, 120, 8, progress, COLOR_PROGRESS);
  
  // 检查是否需要提醒
  if (currentTime - last_drink_reminder >= DRINK_REMINDER_INTERVAL_S) {
    show_reminder = true;
    reminder_start_time = millis();
    last_drink_reminder = currentTime;
  }
}

// -----------------------------
// 显示错误信息
// -----------------------------
void showWeatherError(const char* msg) {
  tft.fillScreen(COLOR_BG_MAIN);
  tft.setTextColor(COLOR_TEXT_ERROR, COLOR_BG_MAIN);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Error:", tft.width() / 2, tft.height() / 2 - WEATHER_LINE_HEIGHT);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  Serial.println("Error displayed: " + String(msg));
  delay(2000);
  drawWeatherStaticElements();
}

// -----------------------------
// 处理旋转编码器输入（时间调整）
// -----------------------------
void handleEncoderInput() {
  int direction = readEncoder();
  
  if (direction != 0 && timeAdjustMode != MODE_NORMAL) {
    if (timeAdjustMode == MODE_ADJUST_HOUR) {
      rtc_time.hour += direction;
      if (rtc_time.hour < 0) rtc_time.hour = 23;
      if (rtc_time.hour > 23) rtc_time.hour = 0;
    } else if (timeAdjustMode == MODE_ADJUST_MINUTE) {
      rtc_time.minute += direction;
      if (rtc_time.minute < 0) rtc_time.minute = 59;
      if (rtc_time.minute > 59) rtc_time.minute = 0;
    }
    drawWeatherTime();
  }
}

// -----------------------------
// 处理按钮输入（模式切换）
// -----------------------------
void handleButtonInput() {
  if (readButton()) {
    if (timeAdjustMode == MODE_NORMAL) {
      // Suspend weather task and switch to main menu state
      if (weatherTaskHandle != NULL) {
        vTaskSuspend(weatherTaskHandle);
      }
      currentAppState = APP_STATE_MAIN_MENU;
      showMenuConfig(); // Draw the icon menu
    } else {
      // 时间调整模式循环
      timeAdjustMode = (TimeAdjustMode)((timeAdjustMode + 1) % 3);
      if (timeAdjustMode == MODE_NORMAL) {
        // 退出调整模式，不再同步系统时间
      }
      drawWeatherTime();
    }
  }
}

// -----------------------------
// 天气初始化任务
// -----------------------------
void Weather_Init_Task(void *pvParameters) {
  Serial.println("=== Weather Init Task Started ===");
  drawWeatherStaticElements();
  
  // Initialize RTC time (no NTP sync)
  // rtc_time.hour, minute, second are initialized to default 12:00:00
  // or persist from previous runs if using RTC memory.
  // For manual adjustment, we don't need to set it here from system time.
  
  drawWeatherTime();
  updateWeatherDisplay();
  last_drink_reminder = time(nullptr);
  
  Serial.println("Attempting WiFi connection for data update...");
  
  if (connectWiFi()) {
    Serial.println("WiFi connected, updating data...");
    
    // No NTP sync here
    
    if (getWeather()) {
      Serial.println("✅ Weather data retrieved");
      last_weather_update = time(nullptr);
      updateWeatherDisplay();
    }
    
    disconnectWiFi();
  }
  
  Serial.println("=== Weather Init Task Completed ===");
  vTaskDelete(NULL);
}

// -----------------------------
// 主界面显示任务
// -----------------------------
void Weather_Task(void *pvParameters) {
  Serial.println("=== Main Screen Task Started ===");
  
  unsigned long lastSecondUpdate = millis();
  
  for (;;) {
    // 处理编码器输入
    handleEncoderInput();
    
    // 处理按钮输入
    handleButtonInput();
    
    // 每秒更新一次时间
    if (millis() - lastSecondUpdate >= 1000) {
      lastSecondUpdate = millis();
      updateRTCtime();
      drawWeatherTime();
      updateDrinkProgress();
    }
    
    // 绘制喝水提醒
    drawDrinkReminder();
    
    // 检查是否需要更新网络数据
    time_t now = time(nullptr);
    bool needUpdate = (now - last_weather_update >= WEATHER_UPDATE_INTERVAL_S);
    
    if (needUpdate && WiFi.status() != WL_CONNECTED) {
      if (connectWiFi()) {
        if (getWeather()) {
          last_weather_update = now;
          updateWeatherDisplay();
        }
        
        disconnectWiFi();
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// -----------------------------
// 主界面入口
// -----------------------------
void weatherMenu() {
  Serial.println("Entering Main Screen");
  enterMenu("MAIN SCREEN");
  
  timeAdjustMode = MODE_NORMAL;
  
  // 创建初始化任务
  xTaskCreatePinnedToCore(Weather_Init_Task, "Weather_Init", 8192, NULL, 2, NULL, 0);
  delay(1000); // 等待初始化完成
  
  // 创建主显示任务
  TaskHandle_t weatherTask = NULL;
  xTaskCreatePinnedToCore(Weather_Task, "Weather_Show", 8192, NULL, 1, &weatherTask, 0);
  
  // weatherMenu is now the main application entry point, it should not return.
  // The main loop is handled by Weather_Task.
  // This function effectively becomes the setup for the main application loop.
  // It should not have its own blocking loop here.
  return;
}