#include <WiFi.h>

// 请修改为您的WiFi网络信息
const char* ssid = "xiaomiao_hotspot";
const char* password = "xiaomiao123";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nESP32C3 WiFi测试程序");
  Serial.println("====================");
  
  // 初始化WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(2000);
  
  // 扫描可用网络
  Serial.println("\n开始扫描网络...");
  int numNetworks = WiFi.scanNetworks();
  
  if (numNetworks == 0) {
    Serial.println("未找到任何网络!");
  } else {
    Serial.printf("找到 %d 个网络:\n", numNetworks);
    Serial.println("序号 | SSID                             | RSSI | 加密方式 | MAC地址");
    Serial.println("-------------------------------------------------------------------");
    
    for (int i = 0; i < numNetworks; i++) {
      Serial.printf("%2d   | %-32s | %4d | %-8s | %s\n", 
                   i + 1, 
                   WiFi.SSID(i).c_str(), 
                   WiFi.RSSI(i), 
                   getEncryptionType(WiFi.encryptionType(i)), 
                   WiFi.BSSIDstr(i).c_str());
      delay(10);
    }
  }
  
  // 尝试连接到指定WiFi
  Serial.println("\n尝试连接到WiFi...");
  Serial.printf("SSID: %s\n", ssid);
  
  WiFi.begin(ssid, password);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // 15秒后超时
    if (millis() - startTime > 15000) {
      Serial.println("\n连接超时!");
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n连接成功!");
    Serial.printf("本地IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("网关IP: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("子网掩码: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("MAC地址: %s\n", WiFi.macAddress().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n连接失败!");
  }
}

void loop() {
  // 每10秒显示一次连接状态
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10000) {
    lastCheck = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[%lu] 连接正常, RSSI: %d dBm, IP: %s\n", 
                   lastCheck, 
                   WiFi.RSSI(), 
                   WiFi.localIP().toString().c_str());
    } else {
      Serial.printf("[%lu] 连接断开\n", lastCheck);
      
      // 尝试重新连接
      WiFi.reconnect();
    }
  }
  
  delay(100);
}

// 获取加密类型字符串
const char* getEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:
      return "开放";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2企业";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    default:
      return "未知";
  }
}