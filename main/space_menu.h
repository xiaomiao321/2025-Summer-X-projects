#ifndef SPACE_MENU_H
#define SPACE_MENU_H

// --- 主屏幕函数 ---
// 这是宇宙菜单的入口和主循环
void SpaceMenuScreen();


// --- 指令函数 ---

// 指令：切换到下一页
void space_menu_next_page();

// 指令：切换到上一页
void space_menu_prev_page();

// 指令：处理“返回”或“退出”按钮
// 返回 true 表示已成功退出，false 表示仍在菜单中
bool space_menu_back_press();


// --- 核心功能函数 ---

// 绘制当前页面的函数
void space_menu_draw();

#endif // SPACE_MENU_H
