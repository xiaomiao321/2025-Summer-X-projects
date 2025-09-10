#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>  // 必须安装此库

// 替换为你的 WiFi 名称和密码
const char* ssid = "xiaomiao_hotspot";
const char* password = "xiaomiao123";

void setup() {
  Serial.begin(115200);
  delay(10);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // 连接 WiFi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setInsecure();  // 跳过证书验证（适用于 Let's Encrypt）

  HTTPClient https;

  if (https.begin(*client, "https://ilovestudent.cn/api/commonApi/randomEnWord")) {
    Serial.println("\n[HTTPS] Connected to server");

    int httpCode = https.GET();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        Serial.println("Raw Response:");
        Serial.println(payload);

        // 开始解析 JSON
        parseWordData(payload);
      } else {
        Serial.printf("HTTP Error Code: %d\n", httpCode);
        Serial.printf("Error: %s\n", https.errorToString(httpCode).c_str());
      }
    } else {
      Serial.printf("Request failed. Error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.println("Connection failed");
  }

  delete client;

  delay(15000); // 每 15 秒请求一次（避免过于频繁）
}

// 解析单词数据
void parseWordData(const String& jsonStr) {
  // 使用 StaticJsonDocument 或 DynamicJsonDocument
  // 根据返回内容大小，这里建议用 Dynamic，避免栈溢出
  DynamicJsonDocument doc(2048); // 足够容纳你的 JSON 数据

  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // 提取字段
  int code = doc["code"];
  const char* msg = doc["msg"];

  if (code != 200) {
    Serial.printf("API Error: %s\n", msg);
    return;
  }

  JsonObject data = doc["data"];
  const char* word = data["headWord"];           // 单词
  const char* tranCn = data["tranCn"];           // 简明中文
  const char* usPhone = data["usPhone"];         // 美式音标
  const char* ukPhone = data["ukPhone"];         // 英式音标

  // 输出基本信息
  Serial.println("\n=== 单词卡片 ===");
  Serial.printf("单词: %s\n", word);
  Serial.printf("中文: %s\n", tranCn);
  Serial.printf("美式音标: %s\n", usPhone);
  Serial.printf("英式音标: %s\n", ukPhone);

  // 解析 trans 数组（详细释义）
  Serial.println("\n详细释义:");
  JsonArray trans = data["trans"];
  for (JsonObject item : trans) {
    const char* pos = item["pos"];
    const char* meaning = item["tranCn"];
    Serial.printf("  [%s] %s\n", pos, meaning);
  }

  // 解析 phrases（常用短语）
  Serial.println("\n常用短语:");
  JsonArray phrases = data["phrases"];
  for (JsonObject item : phrases) {
    const char* content = item["content"];
    const char* cn = item["cn"];
    Serial.printf("  • %s —— %s\n", content, cn);
  }

  // 解析 sentences（例句）
  Serial.println("\n例句:");
  JsonArray sentences = data["sentences"];
  for (JsonObject item : sentences) {
    const char* content = item["content"];
    const char* cn = item["cn"];
    Serial.printf("  🌟 %s\n     → %s\n", content, cn);
  }

  Serial.println("================\n");
}