#ifndef CONFIG_H
#define CONFIG_H

// WiFi and API
const char *WIFI_SSID = "xiaomiao_hotspot";
const char *WIFI_PASSWORD = "xiaomiao123";
const char *WEATHER_API_URL =
    "https://restapi.amap.com/v3/weather/"
    "weatherInfo?city=120104&key=8a4fcc66268926914fff0c968b3c804c";
const char *NTP_SERVER = "ntp.aliyun.com";
const long GMT_OFFSET_SEC = 8 * 3600;
const int DAYLIGHT_OFFSET = 0;

// Intervals
const long SENSOR_INTERVAL_MS = 5000;
const long WEATHER_INTERVAL_S = 3 * 3600;
const long TIME_SYNC_INTERVAL_S = 6 * 3600;

// TFT Layout (240x240 screen)
#define LOGO_X 10
#define LOGO_Y_TOP 10
#define LOGO_Y_BOTTOM 60
#define DATA_X 60
#define DATA_Y 10
#define LINE_HEIGHT 18
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR TFT_BLACK
#define TITLE_COLOR TFT_YELLOW
#define VALUE_COLOR TFT_CYAN
#define ERROR_COLOR TFT_RED
#define MENU_X 10
#define MENU_Y 0
#define MENU_WIDTH 220
#define MENU_HEIGHT 20

// Serial Buffer
#define BUFFER_SIZE 512

#endif