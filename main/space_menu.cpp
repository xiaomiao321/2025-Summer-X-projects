#include "space_menu.h"
#include "space_api.h"
#include "System.h"
#include <TFT_eSPI.h> 
#include "RotaryEncoder.h"
#include "img.h"

// --- 全局变量 (从其他模块引用) ---
extern TFT_eSPI tft;
extern struct tm timeinfo; // 引用全局时间信息
extern bool getLocalTime(struct tm* info, uint32_t ms); // 引用获取本地时间函数
extern struct tm g_last_successful_update_time; // 引用上次成功更新数据的时间

// --- 内部变量 ---
static int g_current_page = 0;
static const int MAX_PAGES = 2;

// --- 内部绘图函数声明 ---
static void draw_astronaut_page(const SpaceData* data);
static void draw_iss_page(const SpaceData* data);
static void draw_loading_page();
static void draw_page_indicator();
static void _draw_real_time_clock(); // 新增：绘制实时时钟
static void _draw_status_bar();      // 新增：绘制状态栏

// --- 主屏幕函数 ---
void SpaceMenuScreen() {
    extern void tftClearLog();
    tftClearLog();

    space_api_update();
    space_menu_draw();

    while(true) {
        bool data_just_updated = space_api_update();

        int rotation = readEncoder();
        bool button_pressed = readButton();
        bool needs_redraw = false;

        if (rotation != 0) {
            if (rotation > 0) space_menu_next_page();
            else space_menu_prev_page();
            needs_redraw = true;
        }

        if (button_pressed) {
            if (space_menu_back_press()) {
                break; 
            }
        }

        if (data_just_updated) {
            needs_redraw = true;
        }

        if (needs_redraw) {
            space_menu_draw();
        }

        _draw_real_time_clock(); // 实时更新时钟

        delay(50);
    }
    extern void showMenu();
    showMenu();
}


// --- 指令函数实现 ---

void space_menu_next_page() {
    g_current_page++;
    if (g_current_page >= MAX_PAGES) {
        g_current_page = 0; 
    }
}

void space_menu_prev_page() {
    g_current_page--;
    if (g_current_page < 0) {
        g_current_page = MAX_PAGES - 1; 
    }
}

bool space_menu_back_press() {
    extern void tftClearLog();
    tftClearLog();
    // tftLogInfo("Exiting Space Menu..."); // Exiting is now silent
    return true; 
}

void space_menu_draw() {
    const SpaceData* data = space_api_get_data();

    switch (g_current_page) {
        case 0: draw_astronaut_page(data); break;
        case 1: draw_iss_page(data); break;
        default: break;
    }
    
    draw_page_indicator();
    _draw_status_bar(); // 绘制状态栏
}

// --- 内部绘图函数实现 ---

static void _draw_real_time_clock() {
    char time_str[9]; // HH:MM:SS\0
    getLocalTime(&timeinfo, 0); // 获取最新时间
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);

    tft.setTextDatum(TL_DATUM); // 左上角对齐
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // 黄色字体，黑色背景
    tft.setTextFont(2); // 较小字体
    tft.setTextSize(1);
    tft.drawString(time_str, 5, 5); // 绘制在左上角
}

static void _draw_status_bar() {
    char status_str[15]; // 足够存储状态信息
    strftime(status_str, sizeof(status_str), "%H:%M:%S", &g_last_successful_update_time);

    tft.setTextDatum(TC_DATUM); 
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); // 浅灰色字体
    tft.setTextSize(1);
    tft.setTextFont(2); // 最小字体
    tft.drawString(status_str, 120, 5); // 绘制在右下角
}

static void draw_page_indicator() {
    tft.setTextDatum(TR_DATUM); 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    String indicator = String(g_current_page + 1) + " / " + String(MAX_PAGES);
    tft.setTextSize(1);
    tft.drawString(indicator, 235, 5);
}

static void draw_loading_page() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM); 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Loading Space Data...", 120, 120);
}

static void draw_astronaut_page(const SpaceData* data) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Astronauts In Space", 120, 30);

    tft.setTextFont(7);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    if (data->astronaut_count == -1) {
        tft.drawString("N/A", 120, 85);
    } else {
        tft.drawString(String(data->astronaut_count), 120, 85);
        
        tft.setTextFont(2);
        tft.setTextDatum(TC_DATUM); 
        int list_y_start = 130;
        int line_height = 22;
        for(int i=0; i < data->astronaut_list_count && i < 5; i++) {
            String astronaut_info = String(i+1) + ". " + data->astronauts[i].name + " (" + data->astronauts[i].craft + ")";
            tft.drawString(astronaut_info, 120, list_y_start + (i * line_height));
        }
    }
}

static void draw_iss_page(const SpaceData* data) {
    tft.fillScreen(TFT_BLACK);

    // 1. 绘制你提供的世界地图，垂直居中偏移
    int map_y_offset = (240 - 134) / 2; // (Screen Height - Map Height) / 2 = (240 - 134) / 2 = 52
    tft.pushImage(0, map_y_offset, 240, 134, map_img); // <-- CORRECTED: Using 'map' array and 134 height

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.drawString("ISS Real-time Position", 120, 30);

    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    if (data->astronaut_count == -1) {
        tft.drawString("Data not available", 120, 225);
    } else {
        int iss_x = (int)(120 + (data->iss_lon * (240.0 / 360.0)));
        int iss_y = (int)((90.0 - data->iss_lat) * (134.0 / 180.0)); // <-- CORRECTED: Using 134 height

        tft.fillCircle(iss_x, iss_y + map_y_offset, 4, TFT_RED);
        tft.drawCircle(iss_x, iss_y + map_y_offset, 5, TFT_WHITE);

        String lat_lon_str = "Lat: " + String(data->iss_lat, 2) + " Lon: " + String(data->iss_lon, 2);
        tft.drawString(lat_lon_str, 120, 225);
    }
}
