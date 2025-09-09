#include "space_api.h"
#include "System.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- 内部变量 ---
static SpaceData g_space_data;
static unsigned long g_last_update = 0;
static bool g_first_update = true; // 控制是否为首次更新
const unsigned long UPDATE_INTERVAL = 2 * 60 * 1000; // 2分钟

// --- API URLs ---
const char* ASTRO_API_URL = "http://api.open-notify.org/astros.json";
const char* ISS_NOW_API_URL = "http://api.open-notify.org/iss-now.json";

// --- 全局变量 (从其他模块引用) ---
extern struct tm timeinfo; // 引用全局时间信息
extern bool getLocalTime(struct tm* info, uint32_t ms); // 引用获取本地时间函数
struct tm g_last_successful_update_time; // 上次成功更新数据的时间

// --- 内部函数声明 ---
static void fetchAstronauts();
static void fetchISSPosition();

// --- 公共函数实现 ---

void space_api_init() {
    // tftLogInfo("Space API module initialized.");
}

const SpaceData* space_api_get_data() {
    return &g_space_data;
}

bool space_api_update() {
    if (g_first_update || (millis() - g_last_update > UPDATE_INTERVAL)) {
        if (g_first_update) {
            tftLogInfo("--- Starting Space API Update ---");
        }
        g_last_update = millis();

        fetchAstronauts();
        fetchISSPosition();
        
        if (g_first_update) {
            tftLogSuccess("--- Space API Update Finished ---");
            delay(2000); // 首次更新后暂停2秒
        }
        
        // 记录本次成功更新的时间
        getLocalTime(&g_last_successful_update_time, 0);

        g_first_update = false; // 在首次成功更新后，关闭显式日志
        return true; // 表示成功执行了更新
    }
    return false; // 表示未到更新时间
}

// --- 内部函数实现 ---

static void fetchAstronauts() {
    HTTPClient http;
    if (g_first_update) tftLogInfo("Fetching astronaut data...");
    http.begin(ASTRO_API_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            if (g_first_update) tftLogError("Astronaut JSON failed: " + String(error.c_str()));
            return;
        }

        g_space_data.astronaut_count = doc["number"];
        g_space_data.astronaut_list_count = 0;
        JsonArray people = doc["people"].as<JsonArray>();
        for (JsonObject person : people) {
            if (g_space_data.astronaut_list_count < 15) {
                g_space_data.astronauts[g_space_data.astronaut_list_count].name = person["name"].as<String>();
                g_space_data.astronauts[g_space_data.astronaut_list_count].craft = person["craft"].as<String>();
                g_space_data.astronaut_list_count++;
            }
        }
        if (g_first_update) tftLogDebug("Astronauts: " + String(g_space_data.astronaut_count));
    } else {
        if (g_first_update) tftLogError("Astronaut HTTP failed: " + String(httpCode));
    }
    http.end();
}

static void fetchISSPosition() {
    HTTPClient http;
    if (g_first_update) tftLogInfo("Fetching ISS position...");
    http.begin(ISS_NOW_API_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            if (g_first_update) tftLogError("ISS JSON failed: " + String(error.c_str()));
            return;
        }

        g_space_data.iss_lat = doc["iss_position"]["latitude"];
        g_space_data.iss_lon = doc["iss_position"]["longitude"];
        if (g_first_update) tftLogDebug("ISS Pos: " + String(g_space_data.iss_lat) + ", " + String(g_space_data.iss_lon));
    } else {
        if (g_first_update) tftLogError("ISS HTTP failed: " + String(httpCode));
    }
    http.end();
}

