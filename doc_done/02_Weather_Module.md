# 时钟模块：WiFi、NTP与天气

## 一、引言

时钟模块是多功能时钟项目的核心，它不仅负责设备的网络连接（WiFi），还通过网络时间协议（NTP）同步精确时间，并从互联网获取实时天气信息。本模块旨在提供一个稳定、可靠且易于配置的网络与时间/天气服务，为其他依赖精确时间或天气数据的模块提供基础支持。

## 二、实现思路与技术选型

### 2.1 WiFi 连接管理

为了简化用户配置WiFi的流程，本模块采用了 `WiFiManager` 库。`WiFiManager` 能够自动创建一个AP热点，用户通过手机连接该热点即可进入配置页面，输入WiFi凭据。这种方式极大地提升了用户体验，避免了硬编码WiFi信息或复杂的串口配置。

### 2.2 NTP 时间同步

精确的时间是时钟设备的基础。本模块通过NTP（Network Time Protocol）服务进行时间同步。
- 使用 `configTime` 函数配置时区和NTP服务器。
- 注册 `sntp_set_time_sync_notification_cb` 回调函数，以便在时间同步成功时执行特定操作（例如，更新系统时间标志，或进行首次天气数据获取）。
- `getLocalTime` 函数用于获取当前的本地时间，并存储在 `tm` 结构体中，方便其他模块访问。

### 2.3 天气数据获取

天气数据通过HTTP GET请求从第三方天气API（例如，OpenWeatherMap）获取。
- 使用 `HTTPClient` 库发起HTTP请求。
- 请求中包含API密钥、城市ID等参数。
- 接收到的JSON格式天气数据通过 `ArduinoJson` 库进行解析，提取出关键信息，如温度、湿度、天气描述、天气图标等。
- 天气数据被结构化存储在 `weatherData` 结构体中，便于统一管理和访问。

### 2.4 MQTT 集成（数据发布）

虽然 `weather.cpp` 中没有直接展示天气数据通过MQTT发布的逻辑，但在一个完整的智能设备项目中，通常会将获取到的天气数据发布到MQTT代理，以便其他设备或服务订阅和利用。这体现了模块化和互联互通的设计理念。

### 2.5 错误处理与健壮性

模块内置了基本的错误处理机制，例如：
- WiFi连接失败时的重试。
- NTP同步失败时的等待和重试。
- HTTP请求失败或JSON解析错误时的日志输出。
- 通过 `g_alarm_is_ringing` 标志，确保在闹钟响铃时，天气更新等操作不会干扰用户交互。

## 三、代码展示

### `weather.cpp`

```c++
#include "weather.h"
#include "WiFiManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include "MQTT.h" // For exitSubMenu
#include <time.h>
#include <sntp.h>

// --- Configuration ---
#define WEATHER_API_KEY "YOUR_OPENWEATHERMAP_API_KEY" // 替换为你的OpenWeatherMap API Key
#define CITY_ID "1816670" // 替换为你的城市ID (例如，北京的城市ID)
#define WEATHER_URL "http://api.openweathermap.org/data/2.5/weather?id=" CITY_ID "&appid=" WEATHER_API_KEY "&units=metric&lang=zh_cn"
#define NTP_SERVER1 "ntp.aliyun.com"
#define NTP_SERVER2 "ntp.ntsc.ac.cn"
#define NTP_SERVER3 "pool.ntp.org"

// --- Global Variables ---
struct tm timeinfo; // 全局时间结构体，存储当前时间信息
bool time_synced = false; // 时间是否已同步的标志
WeatherData weatherData; // 全局天气数据结构体

// --- UI State Variables ---
static unsigned long lastWeatherUpdateTime = 0; // 上次天气更新时间戳
static const unsigned long WEATHER_UPDATE_INTERVAL = 10 * 60 * 1000; // 天气更新间隔：10分钟
static unsigned long lastTimeDisplayUpdate = 0; // 上次时间显示更新时间戳
static const unsigned long TIME_DISPLAY_UPDATE_INTERVAL = 1000; // 时间显示更新间隔：1秒

// --- Forward Declarations ---
static void updateWeatherData();
static void drawWeatherUI();

// --- NTP Callback ---
void timeSyncNotificationCallback(struct timeval *tv) {
  Serial.println("NTP time synced!");
  time_synced = true;
  updateWeatherData(); // 时间同步后立即更新天气数据
}

// --- Initialization Functions ---
void initWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP"); // 自动连接之前保存的WiFi，如果失败则创建AP
  Serial.println("WiFi Connected!");
}

void initNTP() {
  sntp_set_time_sync_notification_cb(timeSyncNotificationCallback); // 设置NTP同步回调
  configTime(8 * 3600, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3); // 配置时区和NTP服务器
  Serial.println("NTP Initialized.");
}

// --- Weather Data Update ---
static void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot update weather.");
    return;
  }

  HTTPClient http;
  http.begin(WEATHER_URL);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Weather API Response:");
    Serial.println(payload);

    DynamicJsonDocument doc(2048); // 分配JSON文档内存
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // 解析JSON数据并填充weatherData结构体
    weatherData.temp = doc["main"]["temp"].as<float>();
    weatherData.humidity = doc["main"]["humidity"].as<int>();
    weatherData.description = doc["weather"][0]["description"].as<String>();
    weatherData.icon = doc["weather"][0]["icon"].as<String>();
    weatherData.city = doc["name"].as<String>();
    weatherData.valid = true; // 标记天气数据有效
    Serial.println("Weather data updated successfully.");
  } else {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    weatherData.valid = false; // 标记天气数据无效
  }
  http.end();
  lastWeatherUpdateTime = millis(); // 更新上次天气更新时间
}

// --- UI Drawing ---
static void drawWeatherUI() {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextDatum(MC_DATUM); // 设置文本对齐方式为中心对齐

  // 显示当前时间
  menuSprite.setTextFont(7); // 使用大字体显示时间
  menuSprite.setTextSize(1);
  menuSprite.setTextColor(TFT_WHITE);
  char time_buf[9]; // HH:MM:SS
  strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo);
  menuSprite.drawString(time_buf, 120, 80); // 居中显示时间

  // 显示日期和星期
  menuSprite.setTextFont(1); // 恢复小字体
  menuSprite.setTextSize(2);
  char date_buf[16]; // YYYY-MM-DD
  strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);
  menuSprite.drawString(date_buf, 120, 120); // 居中显示日期

  char wday_buf[8]; // Weekday
  strftime(wday_buf, sizeof(wday_buf), "%a", &timeinfo);
  menuSprite.drawString(wday_buf, 120, 150); // 居中显示星期

  // 显示天气信息
  if (weatherData.valid) {
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_CYAN);
    menuSprite.drawString(weatherData.city, 120, 180); // 城市名称

    menuSprite.setTextColor(TFT_YELLOW);
    menuSprite.drawString(weatherData.description, 120, 200); // 天气描述

    menuSprite.setTextColor(TFT_ORANGE);
    char temp_buf[10];
    sprintf(temp_buf, "%.1f C", weatherData.temp);
    menuSprite.drawString(temp_buf, 120, 220); // 温度
  } else {
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_RED);
    menuSprite.drawString("Weather N/A", 120, 200); // 天气数据不可用
  }

  menuSprite.pushSprite(0, 0); // 将所有绘制内容推送到屏幕
}

// --- Main Menu Function ---
void weatherMenu() {
  // 确保时间已同步，如果未同步则等待
  if (!time_synced) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.drawString("Syncing Time...", 120, 120);
    menuSprite.pushSprite(0, 0);
    unsigned long start_wait_time = millis();
    while (!time_synced && millis() - start_wait_time < 30000) { // 最多等待30秒
      vTaskDelay(pdMS_TO_TICKS(100));
      if (g_alarm_is_ringing) { return; } // 如果闹钟响了，退出
    }
    if (!time_synced) {
      Serial.println("Time sync failed or timed out.");
      // 可以显示错误信息或返回主菜单
    }
  }

  // 首次进入菜单时更新天气数据
  if (lastWeatherUpdateTime == 0 || millis() - lastWeatherUpdateTime >= WEATHER_UPDATE_INTERVAL) {
    updateWeatherData();
  }
  drawWeatherUI(); // 绘制初始UI

  while (true) {
    if (exitSubMenu) { // 检查是否需要退出子菜单
      exitSubMenu = false; // 重置标志
      return; // 退出函数
    }
    if (g_alarm_is_ringing) { return; } // 如果闹钟响了，退出

    // 每秒更新一次时间显示
    if (millis() - lastTimeDisplayUpdate >= TIME_DISPLAY_UPDATE_INTERVAL) {
      getLocalTime(&timeinfo); // 获取最新时间
      drawWeatherUI(); // 重新绘制UI
      lastTimeDisplayUpdate = millis();
    }

    // 定期更新天气数据
    if (millis() - lastWeatherUpdateTime >= WEATHER_UPDATE_INTERVAL) {
      updateWeatherData();
      drawWeatherUI(); // 天气更新后重新绘制UI
    }

    // 处理按键事件（例如，单击退出）
    if (readButton()) {
      tone(BUZZER_PIN, 1500, 50); // 播放按键音
      break; // 退出循环，返回主菜单
    }
    // 处理长按事件（例如，长按强制刷新天气）
    if (readButtonLongPress()) {
      tone(BUZZER_PIN, 2000, 50); // 播放按键音
      updateWeatherData(); // 强制刷新天气
      drawWeatherUI(); // 刷新UI
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延时，避免忙等待
  }
}
```

## 四、代码解读

### 4.1 配置与全局变量

```c++
// --- Configuration ---
#define WEATHER_API_KEY "YOUR_OPENWEATHERMAP_API_KEY" // 替换为你的OpenWeatherMap API Key
#define CITY_ID "1816670" // 替换为你的城市ID (例如，北京的城市ID)
#define WEATHER_URL "http://api.openweathermap.org/data/2.5/weather?id=" CITY_ID "&appid=" WEATHER_API_KEY "&units=metric&lang=zh_cn"
#define NTP_SERVER1 "ntp.aliyun.com"
#define NTP_SERVER2 "ntp.ntsc.ac.cn"
#define NTP_SERVER3 "pool.ntp.org"

// --- Global Variables ---
struct tm timeinfo; // 全局时间结构体，存储当前时间信息
bool time_synced = false; // 时间是否已同步的标志
WeatherData weatherData; // 全局天气数据结构体
```
- `WEATHER_API_KEY` 和 `CITY_ID`: 这些宏定义了OpenWeatherMap API的访问凭证和目标城市。用户需要替换为自己的有效信息。
- `WEATHER_URL`: 拼接了完整的API请求URL，包括城市ID、API Key、单位（摄氏度）和语言（中文）。
- `NTP_SERVERx`: 定义了多个NTP服务器地址，用于时间同步，增加了同步的成功率和健壮性。
- `timeinfo`: `struct tm` 是C/C++标准库中用于存储日期和时间信息的结构体，这里声明为全局变量，方便在整个项目中共享和访问当前时间。
- `time_synced`: 布尔标志，指示NTP时间是否已成功同步。
- `weatherData`: `WeatherData` 结构体（通常在 `weather.h` 中定义）用于存储从API获取的天气信息，如温度、湿度、描述、图标等。声明为全局变量，确保天气数据在不同函数和模块间可访问。

### 4.2 WiFi 和 NTP 初始化

```c++
// --- Initialization Functions ---
void initWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP"); // 自动连接之前保存的WiFi，如果失败则创建AP
  Serial.println("WiFi Connected!");
}

void initNTP() {
  sntp_set_time_sync_notification_cb(timeSyncNotificationCallback); // 设置NTP同步回调
  configTime(8 * 3600, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3); // 配置时区和NTP服务器
  Serial.println("NTP Initialized.");
}

// --- NTP Callback ---
void timeSyncNotificationCallback(struct timeval *tv) {
  Serial.println("NTP time synced!");
  time_synced = true;
  updateWeatherData(); // 时间同步后立即更新天气数据
}
```
- `initWiFi()`: 使用 `WiFiManager` 库的 `autoConnect` 方法，尝试连接到之前保存的WiFi网络。如果连接失败，它会启动一个名为 "AutoConnectAP" 的接入点，用户可以通过手机连接此AP来配置WiFi。
- `initNTP()`:
    - `sntp_set_time_sync_notification_cb(timeSyncNotificationCallback)`: 注册一个回调函数。当NTP时间同步成功时，`timeSyncNotificationCallback` 函数会被调用。
    - `configTime(8 * 3600, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3)`: 配置NTP客户端。`8 * 3600` 表示东八区（北京时间）的UTC偏移量（秒），`0` 表示夏令时偏移量，后面是NTP服务器列表。
- `timeSyncNotificationCallback()`: 当NTP时间同步成功时，此函数被调用。它将 `time_synced` 标志设置为 `true`，并立即调用 `updateWeatherData()` 来获取最新的天气数据，确保时间同步后天气信息也是最新的。

### 4.3 天气数据更新 `updateWeatherData()`

```c++
static void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot update weather.");
    return;
  }

  HTTPClient http;
  http.begin(WEATHER_URL);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Weather API Response:");
    Serial.println(payload);

    DynamicJsonDocument doc(2048); // 分配JSON文档内存
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // 解析JSON数据并填充weatherData结构体
    weatherData.temp = doc["main"]["temp"].as<float>();
    weatherData.humidity = doc["main"]["humidity"].as<int>();
    weatherData.description = doc["weather"][0]["description"].as<String>();
    weatherData.icon = doc["weather"][0]["icon"].as<String>();
    weatherData.city = doc["name"].as<String>();
    weatherData.valid = true; // 标记天气数据有效
    Serial.println("Weather data updated successfully.");
  } else {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    weatherData.valid = false; // 标记天气数据无效
  }
  http.end();
  lastWeatherUpdateTime = millis(); // 更新上次天气更新时间
}
```
- 网络连接检查: 在发起HTTP请求前，首先检查WiFi是否连接。
- `HTTPClient`: 创建 `HTTPClient` 对象，用于发起HTTP GET请求到 `WEATHER_URL`。
- 响应处理:
    - 如果 `httpCode` 为 `HTTP_CODE_OK` (200)，表示请求成功。
    - `http.getString()` 获取响应体（JSON格式）。
    - `DynamicJsonDocument doc(2048)`: 使用 `ArduinoJson` 库创建一个动态大小的JSON文档，分配2KB内存用于解析。
    - `deserializeJson(doc, payload)`: 解析JSON字符串。如果解析失败，打印错误信息并返回。
    - 数据提取: 从解析后的JSON文档中提取温度、湿度、描述、图标和城市名称，并存储到 `weatherData` 结构体中。
    - `weatherData.valid = true`: 标记天气数据已成功获取。
- 错误处理: 如果HTTP请求失败，打印错误信息，并将 `weatherData.valid` 设置为 `false`。
- `http.end()`: 结束HTTP连接，释放资源。
- `lastWeatherUpdateTime`: 记录本次天气更新的时间戳，用于控制下次更新的间隔。

### 4.4 UI 绘制 `drawWeatherUI()`

```c++
static void drawWeatherUI() {
  menuSprite.fillScreen(TFT_BLACK);
  menuSprite.setTextDatum(MC_DATUM); // 设置文本对齐方式为中心对齐

  // 显示当前时间
  menuSprite.setTextFont(7); // 使用大字体显示时间
  menuSprite.setTextSize(1);
  menuSprite.setTextColor(TFT_WHITE);
  char time_buf[9]; // HH:MM:SS
  strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo);
  menuSprite.drawString(time_buf, 120, 80); // 居中显示时间

  // 显示日期和星期
  menuSprite.setTextFont(1); // 恢复小字体
  menuSprite.setTextSize(2);
  char date_buf[16]; // YYYY-MM-DD
  strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);
  menuSprite.drawString(date_buf, 120, 120); // 居中显示日期

  char wday_buf[8]; // Weekday
  strftime(wday_buf, sizeof(wday_buf), "%a", &timeinfo);
  menuSprite.drawString(wday_buf, 120, 150); // 居中显示星期

  // 显示天气信息
  if (weatherData.valid) {
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_CYAN);
    menuSprite.drawString(weatherData.city, 120, 180); // 城市名称

    menuSprite.setTextColor(TFT_YELLOW);
    menuSprite.drawString(weatherData.description, 120, 200); // 天气描述

    menuSprite.setTextColor(TFT_ORANGE);
    char temp_buf[10];
    sprintf(temp_buf, "%.1f C", weatherData.temp);
    menuSprite.drawString(temp_buf, 120, 220); // 温度
  } else {
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_RED);
    menuSprite.drawString("Weather N/A", 120, 200); // 天气数据不可用
  }

  menuSprite.pushSprite(0, 0); // 将所有绘制内容推送到屏幕
}
```
- `menuSprite`: 使用 `TFT_eSPI` 库的 `TFT_eSprite` 对象进行双缓冲绘制，避免屏幕闪烁。所有绘制操作都在 `menuSprite` 上进行，最后通过 `menuSprite.pushSprite(0, 0)` 一次性推送到屏幕。
- 时间显示: 使用 `strftime` 函数将 `timeinfo` 结构体中的时间格式化为字符串，并以大字体显示时分秒，小字体显示日期和星期。
- 天气信息显示: 根据 `weatherData.valid` 标志判断天气数据是否有效。如果有效，则显示城市、天气描述和温度，并使用不同的颜色区分。如果无效，则显示 "Weather N/A"。
- 文本对齐: `menuSprite.setTextDatum(MC_DATUM)` 将文本对齐方式设置为中间对齐，方便居中显示。

### 4.5 主菜单函数 `weatherMenu()`

```c++
void weatherMenu() {
  // 确保时间已同步，如果未同步则等待
  if (!time_synced) {
    menuSprite.fillScreen(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.drawString("Syncing Time...", 120, 120);
    menuSprite.pushSprite(0, 0);
    unsigned long start_wait_time = millis();
    while (!time_synced && millis() - start_wait_time < 30000) { // 最多等待30秒
      vTaskDelay(pdMS_TO_TICKS(100));
      if (g_alarm_is_ringing) { return; } // 如果闹钟响了，退出
    }
    if (!time_synced) {
      Serial.println("Time sync failed or timed out.");
      // 可以显示错误信息或返回主菜单
    }
  }

  // 首次进入菜单时更新天气数据
  if (lastWeatherUpdateTime == 0 || millis() - lastWeatherUpdateTime >= WEATHER_UPDATE_INTERVAL) {
    updateWeatherData();
  }
  drawWeatherUI(); // 绘制初始UI

  while (true) {
    if (exitSubMenu) { // 检查是否需要退出子菜单
      exitSubMenu = false; // 重置标志
      return; // 退出函数
    }
    if (g_alarm_is_ringing) { return; } // 如果闹钟响了，退出

    // 每秒更新一次时间显示
    if (millis() - lastTimeDisplayUpdate >= TIME_DISPLAY_UPDATE_INTERVAL) {
      getLocalTime(&timeinfo); // 获取最新时间
      drawWeatherUI(); // 重新绘制UI
      lastTimeDisplayUpdate = millis();
    }

    // 定期更新天气数据
    if (millis() - lastWeatherUpdateTime >= WEATHER_UPDATE_INTERVAL) {
      updateWeatherData();
      drawWeatherUI(); // 天气更新后重新绘制UI
    }

    // 处理按键事件（例如，单击退出）
    if (readButton()) {
      tone(BUZZER_PIN, 1500, 50); // 播放按键音
      break; // 退出循环，返回主菜单
    }
    // 处理长按事件（例如，长按强制刷新天气）
    if (readButtonLongPress()) {
      tone(BUZZER_PIN, 2000, 50); // 播放按键音
      updateWeatherData(); // 强制刷新天气
      drawWeatherUI(); // 刷新UI
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延时，避免忙等待
  }
}
```
- 时间同步等待: 进入 `weatherMenu` 时，如果时间尚未同步，会显示 "Syncing Time..." 并等待最多30秒。这确保了后续的时间和天气显示基于准确的时间源。
- 天气数据更新逻辑:
    - 首次进入菜单或距离上次更新超过 `WEATHER_UPDATE_INTERVAL` (10分钟) 时，调用 `updateWeatherData()` 获取最新天气。
    - 循环中，每秒钟调用 `getLocalTime(&timeinfo)` 获取最新时间并刷新UI，确保时间显示实时更新。
    - 循环中，每隔 `WEATHER_UPDATE_INTERVAL` 再次调用 `updateWeatherData()` 刷新天气数据。
- 用户交互:
    - `readButton()`: 检测到单击事件时，播放按键音并退出 `weatherMenu`，返回主菜单。
    - `readButtonLongPress()`: 检测到长按事件时，播放按键音，强制调用 `updateWeatherData()` 刷新天气，并更新UI。
- `exitSubMenu` 和 `g_alarm_is_ringing`: 这两个全局标志用于控制子菜单的退出。`exitSubMenu` 通常由主菜单逻辑设置，表示用户希望返回主菜单。`g_alarm_is_ringing` 是闹钟模块的全局标志，如果闹钟响铃，则立即退出当前子菜单，避免干扰。
- `vTaskDelay`: 引入短暂延时，避免CPU空转，降低功耗。

## 五、总结与展望

时钟模块成功集成了WiFi连接、NTP时间同步和天气数据获取功能，为多功能时钟提供了坚实的基础。模块设计考虑了用户体验（WiFiManager）、数据准确性（NTP）和信息丰富性（天气API）。

未来的改进方向：
1.  更丰富的UI: 可以根据天气图标 (`weatherData.icon`) 加载并显示对应的天气动画或图片，提升视觉效果。
2.  多城市支持: 允许用户配置多个城市，并在菜单中切换显示不同城市的天气。
3.  天气预报: 除了当前天气，还可以集成未来几天的天气预报功能。
4.  更灵活的配置: 将API Key、城市ID、NTP服务器等配置项通过UI界面进行设置，而不是硬编码。
5.  离线模式: 在无网络连接时，能够显示上次成功获取的天气数据，并提示网络状态。
6.  错误提示优化: 当网络或API出现问题时，提供更友好的错误提示信息。

谢谢大家
