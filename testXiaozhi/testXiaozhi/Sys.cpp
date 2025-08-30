#include "Sys.h"
#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
int tft_log_y = 40;
int current_log_lines = 0;
extern char wifiStatusStr[30];
const char* ssid     = "xiaomiao_hotspot";
const char* password = "xiaomiao123";
bool wifi_connected = false;
void tftClearLog() {
    tft.fillRect(0, 30, tft.width(), tft.height() - 30, TFT_BLACK);
    tft_log_y = 40;
    current_log_lines = 0;
}
void typeWriterEffect(const char* text, int x, int y, uint32_t color = TFT_WHITE, int delayMs = 30, bool playSound = false) {
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(1);
    
    for(int i = 0; text[i] != '\0'; i++) {
        tft.drawChar(text[i], x + i * 6, y);
        if (playSound) {
            tone(BUZZER_PIN, 800 + (i % 5) * 100, 10);
        }
        delay(delayMs);
    }
}
// 快速打字机日志函数（无时间戳）
void tftLog(String text, uint16_t color) {
    // 检查是否需要清屏滚动
    if (current_log_lines >= LOG_MAX_LINES) {
        tftClearLog();
    }
    
    // 清除当前行区域
    tft.fillRect(LOG_MARGIN, tft_log_y, tft.width() - LOG_MARGIN * 2, LOG_LINE_HEIGHT, TFT_BLACK);
    
    // 组合提示符和文本
    String fullText = "> " + text;
    
    // 使用typeWriterEffect显示完整内容
    typeWriterEffect(fullText.c_str(), LOG_MARGIN, tft_log_y, color, 8, true);
    
    tft_log_y += LOG_LINE_HEIGHT;
    current_log_lines++;
}
bool connectWiFi() {
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
                tftLog("Error: SSID not found", TFT_RED);
                Serial.printf("Error: SSID not found");
                break;
            case WL_CONNECT_FAILED:
                tftLog("Error: Connect failed", TFT_RED);
                Serial.printf("Error: Connection failed");
                break;
            case WL_CONNECTION_LOST:
                tftLog("Error: Connection lost", TFT_RED);
                Serial.printf("Error: Connection lost");
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
        tft.drawString("WiFi Failed", 120, 60);
        tft.setTextSize(1);
        tft.drawString("Status: " + String(WiFi.status()), 120, 90);
        tft.drawString("Check SSID/Password", 120, 110);
        tft.drawString("Attempts: " + String(attempts), 120, 130);
        
        wifi_connected = false;
        tftLog("========= WiFi Failed =========",TFT_RED);
        Serial.printf("=== WiFi Connection Failed ===");
        delay(2000);
        return false;
    }
}

bool ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        strcpy(wifiStatusStr, "WiFi: Connected");
        return true;
    }

    strcpy(wifiStatusStr, "WiFi: Connecting...");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) { // Try for 10 seconds
        delay(500); // Small delay to allow connection
        // Update status string with progress
        if (millis() % 1000 < 500) {
            strcpy(wifiStatusStr, "WiFi: Connecting.");
        } else {
            strcpy(wifiStatusStr, "WiFi: Connecting..");
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        strcpy(wifiStatusStr, "WiFi: Connected");
        return true;
    } else {
        strcpy(wifiStatusStr, "WiFi: Failed");
        return false;
    }
}
