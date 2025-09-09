#include "RotaryEncoder.h"
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite menuSprite = TFT_eSprite(&tft);

// 定义显示位置
#define TITLE_X 10
#define TITLE_Y 10
#define EVENT_X 10
#define EVENT_Y 40
#define HISTORY_X 10
#define HISTORY_Y 70
#define LINE_HEIGHT 20

// 显示持续时间（毫秒）
#define DISPLAY_DURATION 3000

// 历史记录
#define MAX_HISTORY 5
String eventHistory[MAX_HISTORY];
int historyIndex = 0;

// 状态变量
unsigned long lastEventTime = 0;

void updateDisplay(String title, String currentEvent, String info = "") {
    menuSprite.fillSprite(TFT_BLACK);
    
    // 显示标题
    menuSprite.setTextSize(2);
    menuSprite.drawString(title, TITLE_X, TITLE_Y);
    
    // 显示当前事件（较大字体）
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    menuSprite.drawString(currentEvent, EVENT_X, EVENT_Y);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // 显示附加信息
    if (info != "") {
        menuSprite.setTextSize(1);
        menuSprite.drawString(info, EVENT_X, EVENT_Y + 25);
        menuSprite.setTextSize(2);
    }
    
    // 显示历史记录
    menuSprite.setTextSize(1);
    menuSprite.drawString("Recent events:", HISTORY_X, HISTORY_Y);
    
    int yPos = HISTORY_Y + 15;
    for (int i = 0; i < MAX_HISTORY; i++) {
        int idx = (historyIndex + i) % MAX_HISTORY;
        if (eventHistory[idx] != "") {
            menuSprite.drawString(eventHistory[idx], HISTORY_X + 5, yPos);
            yPos += 12;
        }
    }
    
    // 显示时间
    menuSprite.drawString("Time: " + String(millis() / 1000) + "s", HISTORY_X, 200);
    
    menuSprite.pushSprite(0, 0);
    lastEventTime = millis();
}
void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    menuSprite.createSprite(239, 239);
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2);
    
    // 初始化历史记录
    for (int i = 0; i < MAX_HISTORY; i++) {
        eventHistory[i] = "";
    }
    
    initRotaryEncoder();
    
    // 显示初始界面
    updateDisplay("Rotary Encoder", "Ready", "Waiting for input...");
}

void addToHistory(String event) {
    eventHistory[historyIndex] = event;
    historyIndex = (historyIndex + 1) % MAX_HISTORY;
}



void checkDisplayTimeout() {
    // 如果超过显示持续时间，恢复就绪状态但保留历史
    if (millis() - lastEventTime > DISPLAY_DURATION) {
        updateDisplay("Rotary Encoder", "Ready", "Last: " + eventHistory[(historyIndex + MAX_HISTORY - 1) % MAX_HISTORY]);
    }
}

void loop() {
    // 检查显示超时
    checkDisplayTimeout();

    // 持续调用readButton()以更新状态机
    int clickEvent = readButton();

    // 检查各种事件
    int rotation = readEncoder();
    if (rotation != 0) {
        String direction = (rotation == 1) ? "CW" : "CCW";
        addToHistory("Rotated: " + direction);
        updateDisplay("Encoder Action", "Rotation", "Direction: " + direction);
    }

    if (clickEvent) {
        addToHistory("Single Click");
        updateDisplay("Button Action", "Click", "Single press");
    }

    if (readButtonLongPress()) {
        addToHistory("Long Press");
        updateDisplay("Button Action", "Long Press", "Hold detected");
    }

    if (readDoubleClick()) {
        addToHistory("Double Click");
        updateDisplay("Button Action", "Double Click", "Two quick presses");
    }

    delay(1); // 短暂延时
}