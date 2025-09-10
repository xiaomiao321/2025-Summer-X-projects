#include "weather.h"
#include "menu.h"
#include "MQTT.h"
#include "RotaryEncoder.h"
#include <sys/time.h>
#include <time.h>
#include "Buzzer.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "img.h"
#include "WiFiManager.h"
#include "System.h"
bool synced = false;
// WiFi & Time
const char* ssid     = "xiaomiao";
const char* password = "xiaomiao123";
const char* ntpServer = "ntp.aliyun.com";
const long GMT_OFFSET_SEC = 8 * 3600;
const int DAYLIGHT_OFFSET = 0;

enum WiFiConnectionState {
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED_TEMP, // Temporary failure, will retry
    WIFI_STATE_FAILED_PERM // Permanent failure, e.g., wrong credentials
};

WiFiConnectionState currentWiFiState = WIFI_STATE_IDLE;
unsigned long wifiConnectAttemptStartMillis = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MILLIS = 10000; // 10 seconds timeout for connection attempt
unsigned long lastWiFiRetryMillis = 0;
const unsigned long WIFI_RETRY_DELAY_MILLIS = 30000; // 30 seconds delay before retrying after a failure

// Global Variables
struct tm timeinfo;
bool wifi_connected = false;
char temperature[10] = "N/A";
char humidity[10] = "N/A";
char reporttime[25] = "N/A";
char lastSyncTimeStr[45] = "Never";
char lastWeatherSyncStr[45] = "Never";
char wifiStatusStr[30] = "WiFi: Disconnected"; // Added for real-time status


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
void printLocalTime() {
   if(!getLocalTime(&timeinfo)){
      Serial.printf("FAILED to obtain time");
      return;
   }
}

// Function to ensure WiFi is connected in a non-blocking way
// Returns true if WiFi is currently connected, false otherwise.
// Manages connection attempts internally.
bool ensureWiFiConnected() {
    // 1. If already connected, update status and return true.
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected.");
        // Modified: Include time in status string
        sprintf(wifiStatusStr, "WiFi: Connected at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        wifi_connected = true;
        currentWiFiState = WIFI_STATE_CONNECTED;
        return true;
    }

    // 2. If not connected, manage the connection state machine.
    switch (currentWiFiState) {
        case WIFI_STATE_IDLE:
        case WIFI_STATE_FAILED_TEMP: // If failed temporarily, try again
            // Add retry delay check here
            if (currentWiFiState == WIFI_STATE_FAILED_TEMP && millis() - lastWiFiRetryMillis < WIFI_RETRY_DELAY_MILLIS) {
                // Modified: Include time in status string
                sprintf(wifiStatusStr, "WiFi: Retrying soon at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                return false; // Not time to retry yet
            }

            Serial.println("Initiating WiFi connection...");
            // Modified: Include time in status string
            sprintf(wifiStatusStr, "WiFi: Connecting at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, password); // Non-blocking call
            wifiConnectAttemptStartMillis = millis();
            lastWiFiRetryMillis = millis(); // Update last retry time
            currentWiFiState = WIFI_STATE_CONNECTING;
            wifi_connected = false; // Not connected yet
            return false; // Connection in progress
        
        case WIFI_STATE_CONNECTING:
            // Check for timeout
            if (millis() - wifiConnectAttemptStartMillis > WIFI_CONNECT_TIMEOUT_MILLIS) {
                Serial.println("WiFi connection timed out.");
                // Modified: Include time in status string
                sprintf(wifiStatusStr, "WiFi: TIMEOUT at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                WiFi.disconnect(); // Stop trying for now
                currentWiFiState = WIFI_STATE_FAILED_TEMP; // Allow retry later
                wifi_connected = false;
                lastWiFiRetryMillis = millis(); // Record time of failure
                return false;
            }
            // Still connecting, return false
            // Modified: Include time in status string
            sprintf(wifiStatusStr, "WiFi: Connecting at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec); // Keep status updated
            wifi_connected = false;
            return false;

        case WIFI_STATE_CONNECTED: // Should have been caught by the first if()
            return true;

        case WIFI_STATE_FAILED_PERM: // If permanent failure, don't retry automatically
            // Modified: Include time in status string
            sprintf(wifiStatusStr, "WiFi: Perm FAILED at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            wifi_connected = false;
            return false;
    }
    return false; // Should not reach here
}

bool connectWiFi() {
    if(WiFi.status() == WL_CONNECTED)
    {
        tft.fillScreen(BG_COLOR);
        tft.setTextFont(1);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN);
        tft.drawString("WiFi Already Connected", tft.width()/2, 15);
        tft.setTextSize(2);
        tft.drawString("IP:" + WiFi.localIP().toString(), tft.width()/2, 85);
        tft.drawString("Gateway:" + WiFi.gatewayIP().toString(), tft.width()/2, 155);
        tft.drawString("Signal:" + String(WiFi.RSSI()) + " dBm", tft.width()/2, tft.height()-15);
        delay(2000);
        return true;
    }
    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Scanning WiFi...", 120, 20);
    tft_log_y = 40;

    char log_buffer[100];
    tftLog("========= WiFi Scan Start =========",TFT_YELLOW);
    Serial.printf("\n=== WiFi Scan Start ===");

    // 先扫描所有可用的 WiFi 网络
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(1000);
    
    tftLog("Scanning networks...",TFT_YELLOW);
    Serial.printf("Scanning networks...");
    
    int n = WiFi.scanNetworks();
    sprintf(log_buffer, "Found %d networks", n);
    tftLog(log_buffer, TFT_YELLOW);
    Serial.printf("Found %d networks:\n", n);
    
    // 显示扫描到的网络
    for (int i = 0; i < n && i < 10; i++) { // 最多显示10个网络
        sprintf(log_buffer, "%d: %s (%ddBm)", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        tftLog(log_buffer,TFT_YELLOW);
        Serial.printf("%d: %s (%d dBm) Ch%d\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    }
    
    if (n > 10) {
        sprintf(log_buffer, "... and %d more", n - 10);
        tftLog(log_buffer, TFT_YELLOW);
        Serial.printf("... and %d more networks\n", n - 10);
    }
    
    delay(2000);
    
    // 开始连接过程
    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Connecting WiFi...",120, 20);
    tft_log_y = 40;
    
    sprintf(log_buffer, "SSID: %s", ssid);
    tftLog(log_buffer, TFT_YELLOW);
    Serial.printf("\n=== WiFi Connection Start ===",TFT_YELLOW);
    Serial.printf(log_buffer);

    // 断开之前的连接
    WiFi.disconnect(true);
    delay(1000);
    tftLog("Disconnected previous", TFT_YELLOW);
    Serial.printf("Disconnected previous WiFi connection");
    
    // 配置 WiFi 参数
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    tftLog("WiFi config set", TFT_YELLOW);
    Serial.printf("WiFi configuration set");

    // 显示初始状态
    sprintf(log_buffer, "Init status: %d", WiFi.status());
    tftLog(log_buffer, TFT_YELLOW);
    Serial.printf("Initial WiFi Status: %d\n", WiFi.status());
    
    sprintf(log_buffer, "Init RSSI: %d dBm", WiFi.RSSI());
    tftLog(log_buffer, TFT_YELLOW);
    Serial.printf("Initial RSSI: %d dBm\n", WiFi.RSSI());

    WiFi.begin(ssid, password);
    tftLog("WiFi.begin() called", TFT_YELLOW);
    Serial.printf("WiFi.begin() called");
    
    int attempts = 0;
    int max_attempts = 15;
    
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
        // 更新进度条
        tft.drawRect(20, tft.height() - 20, 202, 17, TFT_WHITE);
        tft.fillRect(21, tft.height() - 18, (attempts + 1) * (200/max_attempts), 13, TFT_GREEN); 

        sprintf(log_buffer, "Attempt %d/%d", attempts + 1, max_attempts);
        tftLog(log_buffer, TFT_YELLOW);
        
        // 详细的连接状态
        sprintf(log_buffer, "Status: %d", WiFi.status());
        tftLog(log_buffer, TFT_YELLOW);
        Serial.printf("\n--- Attempt %d/%d ---\n", attempts + 1, max_attempts);
        Serial.printf("WiFi Status: %d\n", WiFi.status());
        
        sprintf(log_buffer, "RSSI: %d dBm", WiFi.RSSI());
        tftLog(log_buffer,TFT_YELLOW);
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        
        // 显示具体的连接状态信息
        switch(WiFi.status()) {
            case WL_IDLE_STATUS:
                tftLog("State: IDLE", TFT_YELLOW);
                Serial.printf("State: IDLE");
                break;
            case WL_NO_SSID_AVAIL:
                tftLog("ERROR: SSID not found", TFT_RED);
                Serial.printf("ERROR: SSID not found");
                break;
            case WL_CONNECT_FAILED:
                tftLog("ERROR: Connect FAILED", TFT_RED);
                Serial.printf("ERROR: Connection FAILED");
                break;
            case WL_CONNECTION_LOST:
                tftLog("ERROR: Connection lost", TFT_RED);
                Serial.printf("ERROR: Connection lost");
                break;
            case WL_DISCONNECTED:
                tftLog("State: Disconnected", TFT_YELLOW);
                Serial.printf("State: Disconnected");
                break;
            case WL_SCAN_COMPLETED:
                tftLog("State: Scan completed", TFT_YELLOW);
                Serial.printf("State: Scan completed");
                break;
            default:
                sprintf(log_buffer, "State: %d", WiFi.status());
                tftLog(log_buffer, TFT_YELLOW);
                Serial.printf("State: %d\n", WiFi.status());
                break;
        }
        
        delay(1500);
        attempts++;
    }
    tft.fillRect(21, tft.height() - 18, 200, 13, TFT_GREEN);
    tftClearLog();
    tftLog("========= Result =========",TFT_YELLOW);
    Serial.printf("\n=== Connection Result ===");
    
    sprintf(log_buffer, "Final status: %d", WiFi.status());
    tftLog(log_buffer,TFT_YELLOW);
    Serial.printf("Final WiFi Status: %d\n", WiFi.status());

    if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(BG_COLOR);
        tft_log_y = 40;
        // 获取详细的网络信息
        tftLog("WiFi CONNECTED!",TFT_GREEN);
        Serial.printf("WiFi CONNECTED!");
        strcpy(wifiStatusStr, "WiFi: Connected"); // Update status string
        
        sprintf(log_buffer, "IP: %s", WiFi.localIP().toString().c_str());
        tftLog(log_buffer,TFT_GREEN);
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        
        sprintf(log_buffer, "Gateway: %s", WiFi.gatewayIP().toString().c_str());
        tftLog(log_buffer,TFT_GREEN);
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        
        sprintf(log_buffer, "DNS: %s", WiFi.dnsIP().toString().c_str());
        tftLog(log_buffer,TFT_GREEN);
        Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
        
        sprintf(log_buffer, "RSSI: %d dBm", WiFi.RSSI());
        tftLog(log_buffer,TFT_GREEN);
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        
        sprintf(log_buffer, "MAC: %s", WiFi.macAddress().c_str());
        tftLog(log_buffer,TFT_GREEN);
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());

        // 设置 DNS
        WiFi.setDNS(IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
        tftLog("DNS set to 8.8.8.8",TFT_GREEN);
        Serial.printf("DNS set to Google DNS");
        
        delay(2000);
        
        // 显示成功界面
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WiFi Connected", tft.width()/2, 15);
        tft.drawString("IP:" + WiFi.localIP().toString(), tft.width()/2, 85);
        tft.drawString("Gateway:" + WiFi.gatewayIP().toString(), tft.width()/2, 155);
        tft.drawString("Signal:" + String(WiFi.RSSI()) + " dBm", tft.width()/2, tft.height()-15);
        delay(2000);
        
        wifi_connected = true;
        tftLog("========= WiFi Success =========",TFT_GREEN);
        Serial.printf("=== WiFi Connection Successful ===");
        return true;
    } else {
        // 连接失败信息
        sprintf(log_buffer, "FAILED: %d attempts", attempts);
        tftLog(log_buffer,TFT_RED);
        Serial.printf("WiFi Connection FAILED after %d attempts\n", attempts);
        
        sprintf(log_buffer, "Last RSSI: %d dBm", WiFi.RSSI());
        tftLog(log_buffer,TFT_RED);
        Serial.printf("Final RSSI: %d dBm\n", WiFi.RSSI());
        
        // 错误状态信息
        switch(WiFi.status()) {
            case WL_NO_SSID_AVAIL:
                tftLog("SSID not found",TFT_RED);
                Serial.printf("SSID not found in scan results");
                break;
            case WL_CONNECT_FAILED:
                tftLog("Password wrong",TFT_RED);
                Serial.printf("Password may be incorrect");
                break;
            case WL_CONNECTION_LOST:
                tftLog("Connection lost",TFT_RED);
                break;
            default:
                tftLog("Check SSID/Password",TFT_RED);
                break;
        }
        
        delay(3000);

        // 显示失败界面
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WiFi FAILED", 120, 60);
        tft.setTextSize(1);
        tft.drawString("Status: " + String(WiFi.status()), 120, 90);
        tft.drawString("Check SSID/Password", 120, 110);
        tft.drawString("Attempts: " + String(attempts), 120, 130);
        
        wifi_connected = false;
        // tftLog("========= WiFi FAILED =========",TFT_RED);
        Serial.printf("=== WiFi Connection FAILED ===");
        delay(2000);
        return false;
    }
}

void syncTime() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("WiFi not connected, cannot sync time.");
        sprintf(lastSyncTimeStr, "Time FAILED (No WiFi) at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        tft.fillScreen(BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(2);
        tft.setTextSize(2);
        tft.drawString("WiFi Not Connected", 120, 80);
        tft.setTextSize(2);
        tft.drawString("Cannot sync time.", 120, 160);
        tft.setTextSize(2);
        delay(2000);
        tft.setTextFont(1);
        tft.setTextSize(1);
        return;
    }
    if(synced){
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Time Already Synced", tft.width()/2, 15);
        tft.drawString("Last Synced Time:", tft.width()/2, 25);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        const char* weekDayStr[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


        tft.setTextSize(3);
        char time_str_buffer[50];
        tft.drawString(weekDayStr[timeinfo.tm_wday], tft.width()/2, 45); // Y=45
        sprintf(time_str_buffer,"%d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        tft.drawString(time_str_buffer, tft.width()/2, 75);

        sprintf(time_str_buffer,"%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        tft.setTextSize(5);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(time_str_buffer, tft.width()/2, tft.height()/2 + 20);
        delay(2000);      

        return;
    }

    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Syncing Time...", 120, 20);
    tft_log_y = 40;

    char log_buffer[100];
    tftLog("========= NTP Time Sync Start =========",TFT_YELLOW);
    Serial.printf("\n=== NTP Time Sync Start ===");

    sprintf(log_buffer, "NTP Server: %s", ntpServer);
    tftLog(log_buffer,TFT_YELLOW);
    Serial.printf(log_buffer);

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, ntpServer);
    tftLog("configTime() called.",TFT_YELLOW);
    Serial.printf("configTime() called.");

    int attempts = 0;
    int max_attempts = 5;

    while (attempts < max_attempts && !synced) {
        tft.drawRect(20, tft.height() - 20, 202, 17, TFT_WHITE);
        tft.fillRect(21, tft.height() - 18, (attempts + 1) * (200 / max_attempts), 13, TFT_GREEN);

        sprintf(log_buffer, "Attempt %d/%d...", attempts + 1, max_attempts);
        tftLog(log_buffer,TFT_YELLOW);
        Serial.printf("\n--- Attempt %d/%d ---\n", attempts + 1, max_attempts);

        struct tm timeinfo_temp;
        tftLog("Calling getLocalTime()...",TFT_YELLOW);
        Serial.printf("Calling getLocalTime()...");
        if (getLocalTime(&timeinfo_temp, 5000)) { // 5-second timeout
            getLocalTime(&timeinfo); 
            strftime(lastSyncTimeStr, sizeof(lastSyncTimeStr), "Time Success at %H:%M:%S", &timeinfo);
            synced = true;
            tftLog("SUCCESS: Time obtained!",TFT_GREEN);
            Serial.printf("SUCCESS: Time obtained!");
        } else {
            tftLog("FAILED: getLocalTime() timeout.",TFT_RED);
            Serial.printf("FAILED: getLocalTime() timeout.");
        }
        attempts++;
    }
    tftClearLog();
    tftLog("========= Result =========",TFT_YELLOW);
    Serial.printf("\n=== Sync Result ===");

    if (synced) {
        tft.fillRect(21, tft.height() - 18, 200, 13, TFT_GREEN);
        tftLog("Time Synced Successfully.",TFT_GREEN);
        Serial.printf("Time Synced Successfully.");
        
        char time_str_buffer[50];
        strftime(time_str_buffer, sizeof(time_str_buffer), "%A, %Y-%m-%d %H:%M:%S", &timeinfo);
        sprintf(log_buffer, "Current Time", time_str_buffer);
        tftLog(log_buffer,TFT_GREEN);
        sprintf(log_buffer, "%s", time_str_buffer);
        tftLog(log_buffer,TFT_GREEN);
        Serial.printf(log_buffer);
        
        delay(2000);
        tft.fillScreen(BG_COLOR);
        tft.setTextSize(3);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Time Synced", tft.width()/2, 15);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        const char* weekDayStr[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

        tft.drawString(weekDayStr[timeinfo.tm_wday], tft.width()/2, 45); // Y=45
        sprintf(time_str_buffer,"%d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        tft.drawString(time_str_buffer, tft.width()/2, 75);

        sprintf(time_str_buffer,"%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        tft.setTextSize(5);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(time_str_buffer, tft.width()/2, tft.height()/2 + 20);
        delay(2000);
    } else {
        sprintf(lastSyncTimeStr, "Time FAILED at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        tftLog("NTP Sync FAILED.",TFT_RED);
        Serial.printf("NTP Sync FAILED.");
        delay(2000);
        tft.fillScreen(BG_COLOR);
        tft.drawString("NTP Sync FAILED", 120, 80);
        delay(2000);
    }
}

bool fetchWeather() {
    const char* API_KEY  = "8a4fcc66268926914fff0c968b3c804c";
    const char* CITY_CODE = "120104";
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("DEBUG: fetchWeather() called, but wifi_connected is false.");
        sprintf(lastWeatherSyncStr, "Weather FAILED (No WiFi) at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        return false;
    }

    tft.fillScreen(BG_COLOR);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Fetching Weather...", 120, 20);
    tft_log_y = 40;

    strcpy(lastWeatherSyncStr, "Fetching...");
    tftLog("Attempting to fetch weather...",TFT_YELLOW);
    Serial.printf("DEBUG: Attempting to fetch weather...");
    
    WiFiClient client;
    HTTPClient http;
    String url = "http://restapi.amap.com/v3/weather/weatherInfo?city=" + String(CITY_CODE) + "&key=" + String(API_KEY);
    bool success = false;

    String temperature_str = "";
    String humidity_str = "";
    String reporttime_str = "";

    for (int i = 0; i < 5; i++) {
        tft.drawRect(20, tft.height() - 20, 202, 17, TFT_WHITE);
        tft.fillRect(21, tft.height() - 18, (i + 1) * 40, 13, TFT_GREEN); 

        char log_buffer[100];
        sprintf(log_buffer, "Attempt %d/5...", i + 1);
        tftLog(log_buffer,TFT_YELLOW);

        if (http.begin(client, url)) {
            http.setTimeout(5000);
            http.addHeader("User-Agent", "ESP32-Weather");
            http.addHeader("Connection", "close");

            sprintf(log_buffer, "Sending HTTP GET request...");
            tftLog(log_buffer,TFT_YELLOW);
            
            int httpCode = http.GET();
            
            if (httpCode > 0) {
                sprintf(log_buffer, "HTTP Code: %d", httpCode);
                tftLog(log_buffer,TFT_YELLOW);

                if (httpCode == HTTP_CODE_OK) {
                    success = true;
                    String payload = http.getString();
                    tftLog("HTTP Success!",TFT_GREEN);
                    
                    temperature_str = getValue(payload, "\"temperature\":\"", "\"");
                    humidity_str = getValue(payload, "\"humidity\":\"", "\"");
                    reporttime_str = getValue(payload, "\"reporttime\":\"", "\"");

                    if (temperature_str != "N/A") {
                        tftLog("Parse SUCCESS.",TFT_GREEN);
                        sprintf(lastWeatherSyncStr, "Weather Success at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    } else {
                        success = false;
                        tftLog("Parse FAILED. Invalid JSON",TFT_RED);
                        sprintf(lastWeatherSyncStr, "Parse FAILED at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    }
                    
                    http.end();
                    break; 
                } else {
                    sprintf(log_buffer, "HTTP ERROR: %s", http.errorToString(httpCode).c_str());
                    tftLog(log_buffer,TFT_RED);
                }
            } else {
                sprintf(log_buffer, "HTTP ERROR: %s", http.errorToString(httpCode).c_str());
                tftLog(log_buffer,TFT_RED);
            }
            http.end();
        } else {
            tftLog("HTTP begin FAILED",TFT_RED);
        }
        delay(500);
    } 
    if(success)
    {
        tft.fillRect(21, tft.height() - 18, 200, 13, TFT_GREEN);
    }
    if (!success && strcmp(lastWeatherSyncStr, "Fetching...") == 0) {
        sprintf(lastWeatherSyncStr, "weather FAILED (HTTP) at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    delay(2000); 

    tft.fillScreen(BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    if (success) {
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(3);
        tft.drawString("Weather", tft.width()/2, 30);
        tft.drawString("Updated", tft.width()/2, 60);
        tft.setTextSize(2);
        snprintf(temperature, sizeof(temperature), "%sC", temperature_str.c_str());
        snprintf(humidity, sizeof(humidity), "%s%%", humidity_str.c_str());
        reporttime_str.toCharArray(reporttime, sizeof(reporttime)); 
        
        tft.drawString("Temp: " + String(temperature), 120, 110);
        tft.drawString("Humidity: " + String(humidity), 120, 130);
        tft.drawString("ReportTime: " , 120, 150);
        tft.drawString(reporttime_str,120,170);
    } else {
        tft.setTextColor(TFT_RED);
        tft.drawString("Weather FAILED", 120, 80);
        tft.setTextSize(2);
        tft.drawString(lastWeatherSyncStr, 120, 110);
    }
    delay(2000);
    return success;
}

void silentSyncTime() {
    if (!ensureWiFiConnected()) { // Ensure WiFi is connected
        // If ensureWiFiConnected returns false, it means either:
        // 1. Connection is in progress.
        // 2. Connection failed (temporarily or permanently).
        // In either case, we cannot sync time right now.
        // The status string (wifiStatusStr) is already updated by ensureWiFiConnected().
        return;
    }

    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(TR_DATUM);

    strcpy(lastSyncTimeStr, "Syncing Time...");

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, ntpServer);
    if (getLocalTime(&timeinfo, 10000)) {
        strftime(lastSyncTimeStr, sizeof(lastSyncTimeStr), "Time Success at %H:%M:%S", &timeinfo);
        Serial.printf("Silent time sync performed successfully.");
    }
 else {
        sprintf(lastSyncTimeStr, "Time FAILED at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec); // Fixed format string
        Serial.printf("Silent time sync FAILED.");
    }
    // Disconnect WiFi after silent sync attempt
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected after silent time sync.");
}

void silentFetchWeather() {
    if (!ensureWiFiConnected()) { // Ensure WiFi is connected
        // If ensureWiFiConnected returns false, it means either:
        // 1. Connection is in progress.
        // 2. Connection failed (temporarily or permanently).
        // In either case, we cannot fetch weather right now.
        // The status string (wifiStatusStr) is already updated by ensureWiFiConnected().
        return;
    }

    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(TR_DATUM);

    strcpy(lastWeatherSyncStr, "Syncing...");
    
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
            String report_str = getValue(payload, "\"reporttime\":\"", "\"");

            if (temp_str != "N/A" && hum_str != "N/A") {
                snprintf(temperature, sizeof(temperature), "%sC", temp_str.c_str());
                snprintf(humidity, sizeof(humidity), "%s%%", hum_str.c_str());
                report_str.toCharArray(reporttime, sizeof(reporttime));
                sprintf(lastWeatherSyncStr, "Weather Success at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                Serial.printf("Silent weather fetch performed successfully.");
            } else {
                sprintf(lastWeatherSyncStr, "Weather FAILED (Parse) at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                Serial.printf("Silent weather fetch FAILED (parsing).");
            }
        } else {
            sprintf(lastWeatherSyncStr, "Weather FAILED (HTTP) at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            Serial.printf("Silent weather fetch FAILED (HTTP).");
        }
        http.end();
    }
 else {
        sprintf(lastWeatherSyncStr, "Weather FAILED (Connection) at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        Serial.printf("Silent weather fetch FAILED (connection).");
    }
    // Disconnect WiFi after silent weather fetch attempt
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected after silent weather fetch.");
}

void TimeUpdate_Task(void *pvParameters) {
    for(;;) {
        // Only update if time has been synced at least once
        if (synced) {
            getLocalTime(&timeinfo); // Update the global timeinfo struct
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Update every second
    }
}

void weatherMenu() {
    if (exitSubMenu) { exitSubMenu = false; return; }

    connectWiFi(); 
    //bool connected = connectWiFi_with_Manager(); // Connect WiFi when entering the menu
    if (exitSubMenu) { exitSubMenu = false; return; } // Check after trying to connect

    syncTime();
    if (exitSubMenu) { exitSubMenu = false; return; } // Check after trying to sync

    fetchWeather();
    if (exitSubMenu) { exitSubMenu = false; return; } // Check after trying to fetch

    // Disconnect WiFi after initial sync and fetch
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
    // Serial.println("WiFi disconnected after initial setup.");

    // After all operations, or if connection FAILED, proceed to the watchface
    // The watchface itself has an exit loop.
    WatchfaceMenu();
}