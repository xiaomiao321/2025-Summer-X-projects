#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ================== 配置区 ==================
const char* ssid     = "xiaomiao_hotspot";           // 修改为你的 Wi-Fi 名
const char* password = "xiaomiao123";        // 修改为你的 Wi-Fi 密码

// 高德天气 API
const char* API_KEY  = "8a4fcc66268926914fff0c968b3c804c";
const char* CITY_CODE = "120104";

// NTP 时间同步
const char* ntpServer = "ntp.aliyun.com";
const long  gmtOffset_sec = 8 * 3600;         // GMT+8
const int   daylightOffset_sec = 0;

// ==================================================

void networkDiagnostics() {
  Serial.println("\n=== Network Diagnostics ===");
  Serial.printf("WiFi Status: %d\n", WiFi.status());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
  
  Serial.println("Network diagnostics completed");
}

// 使用 HTTP 进行测试（不加密）
void fetchWeatherHTTP() {
  Serial.println("\n=== Testing with HTTP ===");
  
  WiFiClient client;
  HTTPClient http;
  
  String url = "http://restapi.amap.com/v3/weather/weatherInfo?city=" + 
               String(CITY_CODE) + "&key=" + String(API_KEY);
  
  Serial.println("URL: " + url);
  
  if (http.begin(client, url)) {
    http.setTimeout(15000);
    http.addHeader("User-Agent", "ESP32-Weather");
    http.addHeader("Connection", "close");
    
    Serial.println("Sending HTTP GET request...");
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.printf("HTTP Code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("✅ HTTP Success!");
        Serial.println("Response length: " + String(payload.length()));
        
        // 提取天气信息
        String temp = getValue(payload, "\"temperature\":\"", "\"");
        String humi = getValue(payload, "\"humidity\":\"", "\"");
        Serial.println("🌡️  Temperature: " + temp + "°C");
        Serial.println("💧 Humidity: " + humi + "%");
      } else {
        String response = http.getString();
        Serial.println("Response: " + response);
      }
    } else {
      Serial.printf("❌ HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("❌ HTTP begin failed");
  }
}

// 使用 HTTPS 获取天气
void fetchWeatherHTTPS() {
  Serial.println("\n=== Testing with HTTPS ===");
  
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    Serial.println("❌ Failed to create WiFiClientSecure");
    return;
  }

  // 设置 SSL
  client->setInsecure(); // 跳过证书验证
  client->setTimeout(30000);

  HTTPClient https;
  
  // 使用方法1：直接使用完整URL
  String fullURL = "https://restapi.amap.com/v3/weather/weatherInfo?city=" + 
                   String(CITY_CODE) + "&key=" + String(API_KEY);
  
  Serial.println("URL: " + fullURL);
  
  bool connected = https.begin(*client, fullURL);
  
  if (!connected) {
    Serial.println("❌ HTTPS begin failed with full URL");
    delete client;
    return;
  }

  https.setTimeout(30000);
  https.addHeader("User-Agent", "ESP32-Weather");
  https.addHeader("Connection", "close");
  https.addHeader("Host", "restapi.amap.com");

  Serial.println("Sending HTTPS GET request...");
  int httpCode = https.GET();
  
  if (httpCode > 0) {
    Serial.printf("HTTPS Code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      Serial.println("✅ HTTPS Success!");
      Serial.println("Response length: " + String(payload.length()));
      
      // 提取天气信息
      String temp = getValue(payload, "\"temperature\":\"", "\"");
      String humi = getValue(payload, "\"humidity\":\"", "\"");
      Serial.println("🌡️  Temperature: " + temp + "°C");
      Serial.println("💧 Humidity: " + humi + "%");
    } else {
      String response = https.getString();
      Serial.println("Response: " + response);
    }
  } else {
    Serial.printf("❌ HTTPS Error: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();
  delete client;
}

// 工具函数：从字符串提取值
String getValue(String data, String key, String end) {
  int start = data.indexOf(key);
  if (start == -1) return "N/A";
  start += key.length();
  int endIndex = data.indexOf(end, start);
  if (endIndex == -1) return "N/A";
  return data.substring(start, endIndex);
}

void setup() {
  setCpuFrequencyMhz(240);        // 240MHz CPU
  WiFi.setSleep(false);           // 关闭 Wi-Fi 睡眠
  delay(500);
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== HTTPS Weather Test ===");

  // 连接 Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout++ < 40) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ WiFi Connection Failed!");
    return;
  }
  Serial.println("\n✅ WiFi Connected, IP: " + WiFi.localIP().toString());

  // 设置 DNS（使用 Google DNS）
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8));

  // 同步时间
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("⏳ Waiting for NTP time sync...");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("❌ Failed to sync NTP time");
  } else {
    Serial.println("✅ NTP Time: " + String(asctime(&timeinfo)));
  }

  // 网络诊断
  networkDiagnostics();

  // 先尝试 HTTP（更容易成功）
  fetchWeatherHTTP();

  // 等待一下再尝试 HTTPS
  delay(2000);
  fetchWeatherHTTPS();
}

void loop() {
  // 每 5 分钟获取一次天气
  delay(300000);
  fetchWeatherHTTPS();
}