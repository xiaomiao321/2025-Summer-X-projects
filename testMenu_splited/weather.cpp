#include "weather.h"
#include "menu.h"
#include "RotaryEncoder.h"
#include <sys/time.h>
#include <time.h>
#include "Buzzer.h"
#include <WiFi.h>
#include "img.h" // Include image definitions

// WiFi & Time
const char* ssid     = "xiaomiao_hotspot"; // 你的WiFi名称
const char* password = "xiaomiao123";      // 你的WiFi密码
const char* ntpServer = "ntp.aliyun.com";
const long GMT_OFFSET_SEC = 8 * 3600;
const int DAYLIGHT_OFFSET = 0;

// Global Variables
struct tm timeinfo;
WeatherData weatherData;
bool wifi_connected = false;

unsigned long lastDrinkTime = 0;
unsigned long lastMoveTime = 0;
const unsigned long INTERVAL_MS = 30 * 60 * 1000;

// UI Constants
#define BAR_WIDTH 180
#define BAR_HEIGHT 10
#define BAR_START_Y 100
#define BAR_Y_SPACING 30

#define MINUTE_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 2)
#define HOUR_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 1)
#define DAY_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 0)
#define ACTIVITY_BAR_Y (BAR_START_Y + BAR_Y_SPACING * 3) // Combined bar

#define PERCENTAGE_TEXT_X (DATA_X + BAR_WIDTH + 10)

void printLocalTime() {
   if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
   }
   Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

bool connectWiFi() {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TITLE_COLOR, BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Connecting to WiFi", 120, 60);
    tft.setTextSize(1);
    tft.drawString(ssid, 120, 90);

    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) { // 10 attempts
        tft.drawRect(20, 120, 202, 17, TFT_WHITE);
        tft.fillRect(21, 122, attempts * 20, 13, TFT_GREEN); // 20px per attempt
        
        tft.drawRect(20, 150, 202, 10, TFT_WHITE);
        tft.fillRect(22, 152, 200, 6, BG_COLOR);

        unsigned long start = millis();
        while(millis() - start < 2500) { // 2.5 seconds timeout
            if (WiFi.status() == WL_CONNECTED) break;
            float progress = (float)(millis() - start) / 2500.0;
            tft.fillRect(22, 152, (int)(200 * progress), 6, TFT_BLUE);
            delay(50);
        }
        
        if (WiFi.status() == WL_CONNECTED) break;
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        tft.fillRect(21, 122, 200, 13, TFT_GREEN);
        Serial.println(" CONNECTED");
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(2);
        tft.drawString("WiFi Connected", 120, 80);
        tft.setTextSize(1);
        tft.drawString(WiFi.localIP().toString(), 120, 110);
        delay(1000);
        wifi_connected = true;
        return true;
    } else {
        Serial.println(" FAILED");
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(2);
        tft.drawString("WiFi Connect Failed", 120, 80);
        wifi_connected = false;
        delay(2000);
        return false;
    }
}

void syncTime() {
    if (!wifi_connected) return;
    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Syncing time...", 120, 80);
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, ntpServer);
    struct tm timeinfo_temp;
    if (!getLocalTime(&timeinfo_temp, 10000)) {
        Serial.println("Time Sync FAILED");
        tft.fillScreen(BG_COLOR);
        tft.drawString("Time Sync Failed", 120, 80);
        delay(2000);
        return;
    }
    getLocalTime(&timeinfo);
    Serial.println("Time Synced");
    printLocalTime();
    tft.fillScreen(BG_COLOR);
    tft.drawString("Time Synced", 120, 80);
    delay(1000);
}

void getWeather() {
    if (!wifi_connected) {
        weatherData.valid = false;
        return;
    }
    HTTPClient http;
    String url = "https://restapi.amap.com/v3/weather/weatherInfo?city=120104&key=8a4fcc66268926914fff0c968b3c804c";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        if (deserializeJson(doc, payload) == DeserializationError::Ok && doc["status"] == "1") {
            JsonObject lives = doc["lives"][0];
            weatherData.province = lives["province"].as<String>();
            weatherData.city = lives["city"].as<String>();
            weatherData.weather = lives["weather"].as<String>();
            weatherData.temperature = lives["temperature"].as<String>();
            weatherData.humidity = lives["humidity"].as<String>();
            weatherData.reporttime = lives["reporttime"].as<String>();
            weatherData.valid = true;
        } else {
            weatherData.valid = false;
        }
    } else {
        weatherData.valid = false;
    }
    http.end();
}

void drawWeatherOnSprite() {
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.setTextSize(1);
    int y_offset = 60;
    if (wifi_connected) {
        if (weatherData.valid) {
            menuSprite.setTextColor(VALUE_COLOR, BG_COLOR);
            String weather_line1 = weatherData.city + " " + weatherData.weather;
            String weather_line2 = "T:" + weatherData.temperature + "C  H:" + weatherData.humidity + "%";
            menuSprite.drawString(weather_line1, DATA_X, y_offset);
            menuSprite.drawString(weather_line2, DATA_X, y_offset + 18);
        } else {
            menuSprite.setTextColor(ERROR_COLOR, BG_COLOR);
            menuSprite.drawString("Failed to get weather", DATA_X, y_offset);
        }
    } else {
        menuSprite.setTextColor(ERROR_COLOR, BG_COLOR);
        menuSprite.drawString("WIFI not connected", DATA_X, y_offset);
    }
}

void drawScreenOnSprite() {
  char buf[32];
  menuSprite.setTextColor(VALUE_COLOR, BG_COLOR);
  menuSprite.setTextDatum(TL_DATUM);

  // Display Date and Time
  menuSprite.setTextSize(1);
  strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
  menuSprite.drawString(buf, DATA_X + 40, DATA_Y);
  menuSprite.setTextSize(2);
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  menuSprite.drawString(buf, DATA_X + 40, DATA_Y + LINE_HEIGHT);

  // --- Progress Bars ---
  menuSprite.setTextSize(1);
  menuSprite.setTextColor(VALUE_COLOR, BG_COLOR);

  // Minute progress bar
  float minute_progress = (float)timeinfo.tm_sec / 59.0;
  menuSprite.fillRect(DATA_X, MINUTE_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY);
  menuSprite.fillRect(DATA_X, MINUTE_BAR_Y, (int)(BAR_WIDTH * minute_progress), BAR_HEIGHT, TFT_GREEN);
  sprintf(buf, "%.0f%%", minute_progress * 100);
  menuSprite.drawString(buf, PERCENTAGE_TEXT_X, MINUTE_BAR_Y);

  // Hour progress bar
  float hour_progress = (float)(timeinfo.tm_min * 60 + timeinfo.tm_sec) / 3599.0;
  menuSprite.fillRect(DATA_X, HOUR_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY);
  menuSprite.fillRect(DATA_X, HOUR_BAR_Y, (int)(BAR_WIDTH * hour_progress), BAR_HEIGHT, TFT_BLUE);
  sprintf(buf, "%.0f%%", hour_progress * 100);
  menuSprite.drawString(buf, PERCENTAGE_TEXT_X, HOUR_BAR_Y);

  // Day progress bar
  long seconds_in_day = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  float day_progress = (float)seconds_in_day / (24.0 * 3600.0);
  menuSprite.fillRect(DATA_X, DAY_BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY);
  menuSprite.fillRect(DATA_X, DAY_BAR_Y, (int)(BAR_WIDTH * day_progress), BAR_HEIGHT, TFT_RED);
  sprintf(buf, "%.0f%%", day_progress * 100);
  menuSprite.drawString(buf, PERCENTAGE_TEXT_X, DAY_BAR_Y);

  // Combined Activity Bar
  unsigned long elapsedActivityTime = millis() - lastDrinkTime;
  float activity_progress = elapsedActivityTime > INTERVAL_MS ? 1.0 : (float)elapsedActivityTime / INTERVAL_MS;
  menuSprite.pushImage(DATA_X, ACTIVITY_BAR_Y - 7, 24, 24, Performance);
  menuSprite.fillRect(DATA_X + 30, ACTIVITY_BAR_Y, BAR_WIDTH - 30, BAR_HEIGHT, TFT_DARKGREY);
  menuSprite.fillRect(DATA_X + 30, ACTIVITY_BAR_Y, (int)((BAR_WIDTH - 30) * activity_progress), BAR_HEIGHT, TFT_ORANGE);
  sprintf(buf, "%.0f%%", activity_progress * 100);
  menuSprite.drawString(buf, PERCENTAGE_TEXT_X, ACTIVITY_BAR_Y);
}

void Weather_Task(void *pvParameters) {
  unsigned long lastWeatherUpdate = 0;
  for (;;) {
    getLocalTime(&timeinfo);

    menuSprite.fillSprite(BG_COLOR);
    menuSprite.setTextColor(TITLE_COLOR, BG_COLOR);
    menuSprite.setTextSize(1);
    menuSprite.drawString("Time:", DATA_X, DATA_Y);

    drawScreenOnSprite();
    drawWeatherOnSprite();
    
    menuSprite.pushSprite(0, 0);

    if (wifi_connected && (millis() - lastWeatherUpdate > 30 * 60 * 1000)) {
        getWeather();
        lastWeatherUpdate = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void weatherMenu() {
  if (connectWiFi()) {
      syncTime();
      getWeather();
  } else {
      time_t t = time(NULL);
      localtime_r(&t, &timeinfo);
  }
  
  TaskHandle_t weatherTask = NULL;
  xTaskCreatePinnedToCore(Weather_Task, "Weather_Show", 8192, NULL, 1, &weatherTask, 0);

  while (1) {
    if (readButton()) {
      if (detectDoubleClick()) {
        if (weatherTask != NULL) vTaskDelete(weatherTask);
        break;
      } else {
        lastDrinkTime = millis();
        lastMoveTime = millis();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}