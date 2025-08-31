#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>  // 添加WS2812控制库

// WiFi 配置
#define WIFI_SSID "xiaomiao_hotspot"
#define WIFI_PASSWORD "xiaomiao123"

// MQTT 配置
#define MQTT_SERVER "bemfa.com"
#define MQTT_PORT 9501
#define CLIENT_ID "f5ff15f6f91348548bd2038e5d54442e"
#define SUB_TOPIC "Light00"
#define PUB_TOPIC "device/status"

// WS2812 LED 配置
#define LED_PIN 3        // GPIO3连接WS2812
#define LED_COUNT 10      // LED数量，根据实际情况调整

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);  // 创建LED对象

// 回调函数：处理接收到的消息
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("收到消息 [");
  Serial.print(topic);
  Serial.print("]: ");
  
  // 将payload转换为字符串
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // 处理LED控制命令
  if (message.equals("LEDON")) {
    // 打开LED
    setLEDColor(255, 255, 255);  // 白色
    Serial.println("LED已打开");
  } 
  else if (message.equals("LEDOFF")) {
    // 关闭LED
    setLEDColor(0, 0, 0);  // 关闭
    Serial.println("LED已关闭");
  }
}

// 设置LED颜色
void setLEDColor(uint8_t red, uint8_t green, uint8_t blue) {
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(red, green, blue));
  }
  strip.show();  // 更新LED显示
}

// 重连函数
void reconnect() {
  while (!client.connected()) {
    Serial.print("正在连接MQTT...");
    if (client.connect(CLIENT_ID)) {
      Serial.println("成功连接！");
      client.subscribe(SUB_TOPIC); // 订阅主题
    } else {
      Serial.print("连接失败，状态码: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // 初始化LED
  strip.begin();
  strip.show();  // 初始化所有LED为关闭状态
  Serial.println("WS2812 LED初始化完成");
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi已连接");

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();

  // // 定时发送消息
  // static unsigned long lastMsg = 0;
  // if (millis() - lastMsg > 2000) {
  //   lastMsg = millis();
  //   String message = "设备状态正常";
  //   client.publish(PUB_TOPIC, message.c_str());
  //   Serial.println("已发送消息: " + message);
  // }
}