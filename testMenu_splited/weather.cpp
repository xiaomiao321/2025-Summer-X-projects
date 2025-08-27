#include "weather.h"
#include "menu.h"
#include "RotaryEncoder.h"
#include <sys/time.h>
#include <time.h>
#include "Buzzer.h"
#include <WiFi.h>
#include "img.h" // Include image definitions
#include "Watchface.h" // Include Watchface menu

// WiFi & Time
const char* ssid     = "xiaomiao_hotspot"; // 你的WiFi名称
const char* password = "xiaomiao123";      // 你的WiFi密码
const char* ntpServer = "ntp.aliyun.com";
const long GMT_OFFSET_SEC = 8 * 3600;
const int DAYLIGHT_OFFSET = 0;

// Global Variables
struct tm timeinfo; // Make this truly global for Watchface.cpp to access
bool wifi_connected = false;

// UI Constants (simplified for connection status)
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_WHITE
#define DATA_X 20
#define DATA_Y 60
#define LINE_HEIGHT 20
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

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
    
    for (int attempts = 1; attempts <= 10; attempts++) { // Loop for 10 attempts
        tft.fillRect(0, 120, tft.width(), 40, BG_COLOR); // Clear previous attempt info
        tft.setTextSize(1);
        tft.drawString("Attempt " + String(attempts) + "/10", 120, 120);

        // Outer progress bar for attempts
        tft.drawRect(20, 120, 202, 17, TFT_WHITE);
        tft.fillRect(21, 122, attempts * 20, 13, TFT_GREEN);

        unsigned long start = millis();
        while(millis() - start < 2500) { // 2.5 seconds timeout per attempt
            if (WiFi.status() == WL_CONNECTED) break; // Break inner loop if connected
            float progress = (float)(millis() - start) / 2500.0;
            tft.drawRect(20, 150, 202, 10, TFT_WHITE);
            tft.fillRect(22, 152, (int)(200 * progress), 6, TFT_BLUE);
            delay(50);
        }
        
        if (WiFi.status() == WL_CONNECTED) break; // Break outer loop if connected
        delay(500); // Short delay before next attempt
    }

    if (WiFi.status() == WL_CONNECTED) {
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
        // Fallback to internal RTC with specific time
        struct timeval tv = { .tv_sec = 1756386900, .tv_usec = 0 }; // 2025-08-27 15:55:00 GMT+8
        settimeofday(&tv, NULL);
        Serial.println("Using fallback internal RTC time.");
        return;
    }
    getLocalTime(&timeinfo);
    Serial.println("Time Synced");
    printLocalTime();
    tft.fillScreen(BG_COLOR);
    tft.drawString("Time Synced", 120, 80);
    delay(1000);
}

void weatherMenu() {
  if (connectWiFi()) {
      syncTime();
      // If WiFi and time sync successful, enter Watchface menu
      WatchfaceMenu();
  } else {
      // If WiFi fails, initialize timeinfo to a default and proceed to watchface menu
      Serial.println("WiFi connection failed. Using default time.");
      timeinfo.tm_hour = 0;
      timeinfo.tm_min = 0;
      timeinfo.tm_sec = 0;
      timeinfo.tm_mday = 1;
      timeinfo.tm_mon = 0; // January
      timeinfo.tm_year = 2025 - 1900; // Year since 1900
      WatchfaceMenu(); // Proceed to watchface menu with default time
  }
  
  // Always return to main menu config after attempting WiFi/Watchface
  display = 48;
  picture_flag = 0;
  showMenuConfig();
}