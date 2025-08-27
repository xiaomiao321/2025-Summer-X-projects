#include "weather.h"
#include "menu.h"
#include "RotaryEncoder.h"
#include <sys/time.h>
#include <time.h>
#include "Buzzer.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "img.h"
#include "Watchface.h"

// WiFi & Time
const char* ssid     = "xiaomiao_hotspot";
const char* password = "xiaomiao123";
const char* ntpServer = "ntp.aliyun.com";
const long GMT_OFFSET_SEC = 8 * 3600;
const int DAYLIGHT_OFFSET = 0;

// Global Variables
struct tm timeinfo;
bool wifi_connected = false;
char temperature[10] = "N/A";
char humidity[10] = "N/A";
char reporttime[25] = "N/A";
char lastSyncTimeStr[20] = "Never";

// UI Constants
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_WHITE
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED

// Helper function from user reference
String getValue(String data, String key, String end) {
  int start = data.indexOf(key);
  if (start == -1) return "N/A";
  start += key.length();
  int endIndex = data.indexOf(end, start);
  if (endIndex == -1) return "N/A";
  return data.substring(start, endIndex);
}

// Helper for on-screen debug logging
int tft_log_y = 40;
void tftLog(const String& text) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    
    if (tft_log_y > tft.height() - 30) {
        tft_log_y = 40;
        tft.fillRect(0, 30, tft.width(), tft.height() - 60, TFT_BLACK);
    }
    tft.drawString(text, 5, tft_log_y);
    tft_log_y += 10;
}


void printLocalTime() {
   if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
   }
   Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

bool connectWiFi() {
    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Scanning WiFi...", 120, 20);
    tft_log_y = 40;

    char log_buffer[100];
    tftLog("=== WiFi Scan Start ===");
    Serial.println("\n=== WiFi Scan Start ===");

    // 先扫描所有可用的 WiFi 网络
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(1000);
    
    tftLog("Scanning networks...");
    Serial.println("Scanning networks...");
    
    int n = WiFi.scanNetworks();
    sprintf(log_buffer, "Found %d networks", n);
    tftLog(log_buffer);
    Serial.printf("Found %d networks:\n", n);
    
    // 显示扫描到的网络
    for (int i = 0; i < n && i < 5; i++) { // 最多显示5个网络
        sprintf(log_buffer, "%d: %s (%ddBm)", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        tftLog(log_buffer);
        Serial.printf("%d: %s (%d dBm) Ch%d\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    }
    
    if (n > 5) {
        sprintf(log_buffer, "... and %d more", n - 5);
        tftLog(log_buffer);
        Serial.printf("... and %d more networks\n", n - 5);
    }
    
    delay(2000);
    
    // 开始连接过程
    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Connecting to WiFi...", 120, 20);
    tft_log_y = 40;
    
    sprintf(log_buffer, "SSID: %s", ssid);
    tftLog(log_buffer);
    Serial.println("\n=== WiFi Connection Start ===");
    Serial.println(log_buffer);

    // 断开之前的连接
    WiFi.disconnect(true);
    delay(1000);
    tftLog("Disconnected previous");
    Serial.println("Disconnected previous WiFi connection");
    
    // 配置 WiFi 参数
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    tftLog("WiFi config set");
    Serial.println("WiFi configuration set");

    // 显示初始状态
    sprintf(log_buffer, "Init status: %d", WiFi.status());
    tftLog(log_buffer);
    Serial.printf("Initial WiFi Status: %d\n", WiFi.status());
    
    sprintf(log_buffer, "Init RSSI: %d dBm", WiFi.RSSI());
    tftLog(log_buffer);
    Serial.printf("Initial RSSI: %d dBm\n", WiFi.RSSI());

    WiFi.begin(ssid, password);
    tftLog("WiFi.begin() called");
    Serial.println("WiFi.begin() called");
    
    int attempts = 0;
    int max_attempts = 15;
    
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
        // 更新进度条
        tft.drawRect(20, tft.height() - 20, 202, 17, TFT_WHITE);
        tft.fillRect(21, tft.height() - 18, (attempts + 1) * (200/max_attempts), 13, TFT_GREEN); 

        sprintf(log_buffer, "Attempt %d/%d", attempts + 1, max_attempts);
        tftLog(log_buffer);
        
        // 详细的连接状态
        sprintf(log_buffer, "Status: %d", WiFi.status());
        tftLog(log_buffer);
        Serial.printf("\n--- Attempt %d/%d ---\n", attempts + 1, max_attempts);
        Serial.printf("WiFi Status: %d\n", WiFi.status());
        
        sprintf(log_buffer, "RSSI: %d dBm", WiFi.RSSI());
        tftLog(log_buffer);
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        
        // 显示具体的连接状态信息
        switch(WiFi.status()) {
            case WL_IDLE_STATUS:
                tftLog("State: IDLE");
                Serial.println("State: IDLE");
                break;
            case WL_NO_SSID_AVAIL:
                tftLog("Error: SSID not found");
                Serial.println("Error: SSID not found");
                break;
            case WL_CONNECT_FAILED:
                tftLog("Error: Connect failed");
                Serial.println("Error: Connection failed");
                break;
            case WL_CONNECTION_LOST:
                tftLog("Error: Connection lost");
                Serial.println("Error: Connection lost");
                break;
            case WL_DISCONNECTED:
                tftLog("State: Disconnected");
                Serial.println("State: Disconnected");
                break;
            case WL_SCAN_COMPLETED:
                tftLog("State: Scan completed");
                Serial.println("State: Scan completed");
                break;
            default:
                sprintf(log_buffer, "State: %d", WiFi.status());
                tftLog(log_buffer);
                Serial.printf("State: %d\n", WiFi.status());
                break;
        }
        
        delay(1500);
        attempts++;
    }

    tftLog("=== Result ===");
    Serial.println("\n=== Connection Result ===");
    
    sprintf(log_buffer, "Final status: %d", WiFi.status());
    tftLog(log_buffer);
    Serial.printf("Final WiFi Status: %d\n", WiFi.status());

    if (WiFi.status() == WL_CONNECTED) {
        // 获取详细的网络信息
        tftLog("WiFi CONNECTED!");
        Serial.println("WiFi CONNECTED!");
        
        sprintf(log_buffer, "IP: %s", WiFi.localIP().toString().c_str());
        tftLog(log_buffer);
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        
        sprintf(log_buffer, "Gateway: %s", WiFi.gatewayIP().toString().c_str());
        tftLog(log_buffer);
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        
        sprintf(log_buffer, "DNS: %s", WiFi.dnsIP().toString().c_str());
        tftLog(log_buffer);
        Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
        
        sprintf(log_buffer, "RSSI: %d dBm", WiFi.RSSI());
        tftLog(log_buffer);
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        
        sprintf(log_buffer, "MAC: %s", WiFi.macAddress().c_str());
        tftLog(log_buffer);
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());

        // 设置 DNS
        WiFi.setDNS(IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
        tftLog("DNS set to 8.8.8.8");
        Serial.println("DNS set to Google DNS");
        
        delay(2000);
        
        // 显示成功界面
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(2);
        tft.drawString("WiFi Connected", 120, 60);
        tft.setTextSize(1);
        tft.drawString("IP: " + WiFi.localIP().toString(), 120, 90);
        tft.drawString("Gateway: " + WiFi.gatewayIP().toString(), 120, 110);
        tft.drawString("Signal: " + String(WiFi.RSSI()) + " dBm", 120, 130);
        delay(2000);
        
        wifi_connected = true;
        tftLog("=== WiFi Success ===");
        Serial.println("=== WiFi Connection Successful ===");
        return true;
    } else {
        // 连接失败信息
        sprintf(log_buffer, "FAILED: %d attempts", attempts);
        tftLog(log_buffer);
        Serial.printf("WiFi Connection FAILED after %d attempts\n", attempts);
        
        sprintf(log_buffer, "Last RSSI: %d dBm", WiFi.RSSI());
        tftLog(log_buffer);
        Serial.printf("Final RSSI: %d dBm\n", WiFi.RSSI());
        
        // 错误状态信息
        switch(WiFi.status()) {
            case WL_NO_SSID_AVAIL:
                tftLog("SSID not found");
                Serial.println("SSID not found in scan results");
                break;
            case WL_CONNECT_FAILED:
                tftLog("Password wrong?");
                Serial.println("Password may be incorrect");
                break;
            case WL_CONNECTION_LOST:
                tftLog("Connection lost");
                break;
            default:
                tftLog("Check SSID/Password");
                break;
        }
        
        delay(3000);

        // 显示失败界面
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(2);
        tft.drawString("WiFi Failed", 120, 60);
        tft.setTextSize(1);
        tft.drawString("Status: " + String(WiFi.status()), 120, 90);
        tft.drawString("Check SSID/Password", 120, 110);
        tft.drawString("Attempts: " + String(attempts), 120, 130);
        
        wifi_connected = false;
        tftLog("=== WiFi Failed ===");
        Serial.println("=== WiFi Connection Failed ===");
        delay(2000);
        return false;
    }
}

void syncTime() {
    if (!wifi_connected) {
        Serial.println("WiFi not connected, using random time and setting RTC.");
        strcpy(lastSyncTimeStr, "Never");
        timeinfo.tm_year = 2024 - 1900;
        timeinfo.tm_mon = random(12);
        timeinfo.tm_mday = random(1, 29);
        timeinfo.tm_hour = random(24);
        timeinfo.tm_min = random(60);
        timeinfo.tm_sec = random(60);
        time_t t = mktime(&timeinfo);
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        return;
    }

    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Syncing time...", 120, 60);
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, ntpServer);
    int attempts = 0;
    bool synced = false;
    while (attempts < 5) {
        tft.drawRect(20, 120, 202, 17, TFT_WHITE);
        tft.fillRect(21, 122, attempts * 40, 13, TFT_GREEN);
        struct tm timeinfo_temp;
        if (getLocalTime(&timeinfo_temp, 5000)) {
            getLocalTime(&timeinfo);
            strftime(lastSyncTimeStr, sizeof(lastSyncTimeStr), "Synced @%H:%M", &timeinfo);
            synced = true;
            break;
        }
        attempts++;
    }

    if (synced) {
        tft.fillRect(21, 122, 200, 13, TFT_GREEN);
        Serial.println("Time Synced");
        printLocalTime();
        tft.fillScreen(BG_COLOR);
        tft.drawString("Time Synced", 120, 80);
        delay(1000);
    } else {
        strcpy(lastSyncTimeStr, "Failed");
        Serial.println("NTP Sync FAILED, using random time and setting RTC.");
        tft.fillScreen(BG_COLOR);
        tft.drawString("NTP Sync Failed", 120, 80);
        delay(2000);
        timeinfo.tm_year = 2024 - 1900;
        timeinfo.tm_mon = random(12);
        timeinfo.tm_mday = random(1, 29);
        timeinfo.tm_hour = random(24);
        timeinfo.tm_min = random(60);
        timeinfo.tm_sec = random(60);
        time_t t = mktime(&timeinfo);
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }
}

bool fetchWeather() {
    const char* API_KEY  = "8a4fcc66268926914fff0c968b3c804c";
    const char* CITY_CODE = "120104";
    
    if (!wifi_connected) {
        Serial.println("DEBUG: fetchWeather() called, but wifi_connected is false.");
        return false;
    }

    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Fetching Weather...", 120, 20);
    tft_log_y = 40; // Reset log position

    tftLog("Attempting to fetch weather...");
    Serial.println("DEBUG: Attempting to fetch weather...");
    
    WiFiClient client;
    HTTPClient http;
    String url = "http://restapi.amap.com/v3/weather/weatherInfo?city=" + String(CITY_CODE) + "&key=" + String(API_KEY);
    bool success = false;

    // 使用 String 类型来存储温度湿度
    String temperature_str = "";
    String humidity_str = "";
    String reporttime_str = "";

    for (int i = 0; i < 5; i++) {
        // 绘制进度条
        tft.drawRect(20, tft.height() - 20, 202, 17, TFT_WHITE);
        tft.fillRect(21, tft.height() - 18, (i + 1) * 40, 13, TFT_GREEN); 

        char log_buffer[100];
        sprintf(log_buffer, "Attempt %d/5...", i + 1);
        tftLog(log_buffer);

        // 开始 HTTP 连接
        if (http.begin(client, url)) {
            http.setTimeout(15000);
            http.addHeader("User-Agent", "ESP32-Weather");
            http.addHeader("Connection", "close");

            sprintf(log_buffer, "Sending HTTP GET request...");
            tftLog(log_buffer);
            
            int httpCode = http.GET();
            
            if (httpCode > 0) {
                sprintf(log_buffer, "HTTP Code: %d", httpCode);
                tftLog(log_buffer);

                if (httpCode == HTTP_CODE_OK) {
                    success = true;
                    String payload = http.getString();
                    tftLog("HTTP Success!");
                    sprintf(log_buffer, "Response length: %d", payload.length());
                    tftLog(log_buffer);
                    tftLog("Payload received. Parsing...");
                    
                    // 提取天气信息到 String 变量
                    temperature_str = getValue(payload, "\"temperature\":\"", "\"");
                    humidity_str = getValue(payload, "\"humidity\":\"", "\"");
                    reporttime_str = getValue(payload, "\"reporttime\":\"", "\"");
                    sprintf(log_buffer, "Temperature: %s°C", temperature_str.c_str());
                    tftLog(log_buffer);
                    sprintf(log_buffer, "Humidity: %s%%", humidity_str.c_str());
                    tftLog(log_buffer);
                    sprintf(log_buffer, "Report Time: %s", reporttime_str.c_str()); 
                    tftLog(log_buffer);

                    tftLog("Success! Data parsed.");
                    http.end(); // 在成功时结束连接
                    break; 
                } else {
                    sprintf(log_buffer, "HTTP Error: %s", http.errorToString(httpCode).c_str());
                    tftLog(log_buffer);
                    String response = http.getString(); // 获取错误响应
                    tftLog("Response: " + response);
                }
            } else {
                sprintf(log_buffer, "HTTP Error: %s", http.errorToString(httpCode).c_str());
                tftLog(log_buffer);
            }
            http.end(); // 确保每次循环都结束连接
        } else {
            tftLog("HTTP begin failed");
        }
        delay(500); // 重试间隔
    } 
    
    delay(3000); // Let user see the final log status

    tft.fillScreen(BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    if (success) {
        tft.drawString("Weather Updated", 120, 80);
        tft.setTextSize(1);
        
        // 如果全局变量是 char 数组，需要转换
        temperature_str.toCharArray(temperature, sizeof(temperature));
        humidity_str.toCharArray(humidity, sizeof(humidity));
        reporttime_str.toCharArray(reporttime, sizeof(reporttime)); 
        
        tft.drawString("Temp: " + temperature_str, 120, 110);
        tft.drawString("Humidity: " + humidity_str, 120, 130);
        tft.drawString("ReportTime: " + reporttime_str, 120, 140);
    } else {
        tft.drawString("Weather Failed", 120, 80);
    }
    delay(2000);
    return success;
}

void silentSyncTime() {
    if (WiFi.status() != WL_CONNECTED) return;
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, ntpServer);
    if (getLocalTime(&timeinfo, 10000)) {
        strftime(lastSyncTimeStr, sizeof(lastSyncTimeStr), "Synced @%H:%M", &timeinfo);
        Serial.println("Silent time sync performed.");
    }
}

void silentFetchWeather() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    WiFiClient client;
    HTTPClient http;
    String url = "http://restapi.amap.com/v3/weather/weatherInfo?city=120104&key=8a4fcc66268926914fff0c968b3c804c";
    
    if (http.begin(client, url)) {
        http.setTimeout(5000);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            String temp_str = getValue(payload, "\"temperature\":\"", "\"");
            String hum_str = getValue(payload, "\"humidity\":\"", "\"");

            if (temp_str != "N/A" && hum_str != "N/A") {
                snprintf(temperature, sizeof(temperature), "%s C", temp_str.c_str());
                snprintf(humidity, sizeof(humidity), "%s %%", hum_str.c_str());
            }
        }
        http.end();
    }
}

void weatherMenu() {
  connectWiFi();
  syncTime();
  if (wifi_connected) {
    fetchWeather();
  }
  WatchfaceMenu();
}
