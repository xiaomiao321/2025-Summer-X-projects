#ifndef SPACE_API_H
#define SPACE_API_H

#include <Arduino.h>

// 用于存储宇航员信息
struct Astronaut {
    String name;
    String craft;
};

// 统一存储所有太空数据
struct SpaceData {
    // Page 1: Astronauts
    int astronaut_count = -1; // 默认为-1，表示尚未获取
    Astronaut astronauts[15];
    int astronaut_list_count = 0;

    // Page 2: ISS Position
    float iss_lat = 0;
    float iss_lon = 0;
};


// --- 函数声明 ---

// 初始化模块
void space_api_init();

// 更新数据函数，如果实际执行了更新，则返回 true
bool space_api_update();

// 获取指向太空数据的常量指针
const SpaceData* space_api_get_data();

extern struct tm g_last_successful_update_time; // Added: Last successful update time

#endif // SPACE_API_H
