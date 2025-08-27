#include "wff_simple_clock.h"
#include "wff_tft_espi_adapter.h" // 包含我们的适配层API

// 原始的绘图逻辑，几乎没有改动
void drawSimpleClock(void) {
	byte size_w = 5;
	byte size_h = 7;

	setVectorDrawClipWin(0,0,128,64);
	drawVectorChar(3, 8, '0' + div10(timeDate->time.hour), 1, 0, size_w, size_h);
	drawVectorChar(3 + size_w * 6, 8, '0' + mod10(timeDate->time.hour), 1, 0, size_w, size_h);
	
	drawVectorChar(6 + 64, 8, '0' + div10(timeDate->time.mins), 1, 0, size_w, size_h);
	drawVectorChar(6 + 64 + size_w * 6, 8, '0' + mod10(timeDate->time.mins), 1, 0, size_w, size_h);

	// 闪烁的冒号
	if(millis_get() % 1000 > 500) {
		drawVectorChar(64 - 3 * 6 / 2, -3, '.', 1, 0, 3, 3);
		drawVectorChar(64 - 3 * 6 / 2, 0 + 32, '.', 1, 0, 3, 3);
	}
}

// 初始化函数
void simple_clock_init(void) {
    // 原始代码中这个函数是空的，保留以备将来使用
}

// 绘图主函数
void simple_clock_draw(void) {
	watchFace_switchUpdate(); // 调用适配层的函数
	drawSimpleClock();
}
