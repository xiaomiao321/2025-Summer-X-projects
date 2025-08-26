#include <WiFi.h>

const char* ssid     = "xiaomiao_hotspot"; // 你的WiFi名称
const char* password = "xiaomiao123";      // 你的WiFi密码

void setup()
{
    Serial.begin(115200);
    delay(10);

    // 连接WiFi网络
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    // 每隔5秒输出一次WiFi状态
    Serial.println("-------------------------------");
    Serial.print("WiFi Status: ");
    
    switch(WiFi.status()) {
        case WL_CONNECTED:
            Serial.println("Connected");
            Serial.print("SSID: ");
            Serial.println(WiFi.SSID());
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.print("Signal Strength (RSSI): ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
            break;
        case WL_DISCONNECTED:
            Serial.println("Disconnected");
            break;
        case WL_CONNECTION_LOST:
            Serial.println("Connection Lost");
            break;
        case WL_CONNECT_FAILED:
            Serial.println("Connect Failed");
            break;
        default:
            Serial.println("Unknown");
            break;
    }

    Serial.println("-------------------------------");
    
    delay(5000); // 每5秒打印一次
}