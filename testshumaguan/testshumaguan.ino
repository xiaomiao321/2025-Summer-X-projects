#include <Arduino.h>
#include <TFT_eSPI.h>

// 定义常量
#define TICKER_GAP 10
#define TIME_MAIN_COLOR TFT_WHITE
#define TIME_TENTH_COLOR TFT_WHITE

// 字体配置 - 分别设置不同部分的字体和大小
#define HOUR_FONT 7        // 小时字体编号
#define HOUR_SIZE 1        // 小时字体大小倍数
#define MINUTE_FONT 7      // 分钟字体编号  
#define MINUTE_SIZE 1      // 分钟字体大小倍数
#define SECOND_FONT 7      // 秒钟字体编号
#define SECOND_SIZE 1      // 秒钟字体大小倍数
#define TENTH_FONT 4       // 0.1秒字体编号
#define TENTH_SIZE 1       // 0.1秒字体大小倍数
#define COLON_FONT 7       // 冒号字体编号
#define COLON_SIZE 1       // 冒号字体大小倍数

// 定义结构体
struct time_s {
    uint8_t hour;
    uint8_t mins;
    uint8_t secs;
    uint8_t tenth; // 0.1秒 (0-9)
};

struct TickerData {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint8_t val;
    uint8_t max_val;
    bool moving;
    int16_t y_pos;
    uint8_t font;      // 字体编号
    uint8_t textSize;  // 字体大小倍数
};

// 全局变量
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite menuSprite = TFT_eSprite(&tft);
time_s g_watchface_timeDate = {12, 34, 56, 0}; // 初始时间 12:34:56.0

// 函数声明
static void drawVectorScrollTickerNum(TickerData* data);
void drawCommonElements();
void VectorScrollWatchface();
void updateTime();

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    menuSprite.createSprite(239, 239);
    
    // 设置文本属性
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.setTextColor(TIME_MAIN_COLOR, TFT_BLACK);
}

void loop() {
    // 模拟时间变化
    updateTime();
    
    // 绘制表盘
    VectorScrollWatchface();
    delay(20);
}

void updateTime() {
    // 模拟时间更新
    static unsigned long lastUpdate = 0;
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate >= 100) { // 每0.1秒更新一次
        lastUpdate = currentMillis;
        g_watchface_timeDate.tenth++;
        if (g_watchface_timeDate.tenth >= 10) {
            g_watchface_timeDate.tenth = 0;
            g_watchface_timeDate.secs++;
            if (g_watchface_timeDate.secs >= 60) {
                g_watchface_timeDate.secs = 0;
                g_watchface_timeDate.mins++;
                if (g_watchface_timeDate.mins >= 60) {
                    g_watchface_timeDate.mins = 0;
                    g_watchface_timeDate.hour++;
                    if (g_watchface_timeDate.hour >= 24) {
                        g_watchface_timeDate.hour = 0;
                    }
                }
            }
        }
        Serial.printf("Time: %02d:%02d:%02d.%d\n", 
                     g_watchface_timeDate.hour, 
                     g_watchface_timeDate.mins, 
                     g_watchface_timeDate.secs,
                     g_watchface_timeDate.tenth);
    }
}

static void drawVectorScrollTickerNum(TickerData* data) {
    int16_t y_pos = data->y_pos;
    int16_t x = data->x, y = data->y, h = data->h;
    uint8_t prev_val = (data->val == 0) ? data->max_val : data->val - 1;
    char current_char[2] = {(char)('0' + data->val), '\0'};
    char prev_char[2] = {(char)('0' + prev_val), '\0'};

    // 设置字体编号和大小
    menuSprite.setTextFont(data->font);
    menuSprite.setTextSize(data->textSize);

    if (!data->moving || y_pos == 0 || y_pos > h + TICKER_GAP) {
        menuSprite.drawString(current_char, x, y);
        return;
    }

    // 绘制从底部滚动进来的新数字
    int16_t new_y = y + h + TICKER_GAP - y_pos;
    menuSprite.drawString(current_char, x, new_y);

    // 绘制向上滚出的旧数字
    int16_t old_y = y - y_pos;
    menuSprite.drawString(prev_char, x, old_y);
}

void drawCommonElements() {
    // 这里可以添加表盘的公共元素
    // 例如：边框、日期、电池状态等
}

void VectorScrollWatchface() {
    static time_s last_time = {255, 255, 255, 255};
    static TickerData tickers[7]; // 6个时间数字 + 1个0.1秒数字
    static bool firstRun = true;
    
    // 动态计算字符尺寸（使用默认大小1）
    menuSprite.setTextFont(HOUR_FONT);
    menuSprite.setTextSize(1);
    int hour_num_w = menuSprite.textWidth("0");
    int hour_num_h = menuSprite.fontHeight();
    
    menuSprite.setTextFont(MINUTE_FONT);
    int min_num_w = menuSprite.textWidth("0");
    int min_num_h = menuSprite.fontHeight();
    
    menuSprite.setTextFont(SECOND_FONT);
    int sec_num_w = menuSprite.textWidth("0");
    int sec_num_h = menuSprite.fontHeight();
    
    menuSprite.setTextFont(TENTH_FONT);
    int tenth_num_w = menuSprite.textWidth("0");
    int tenth_num_h = menuSprite.fontHeight();
    
    menuSprite.setTextFont(COLON_FONT);
    int colon_w = menuSprite.textWidth(":");
    int dot_w = menuSprite.textWidth(".");
    
    // 计算起始位置（居中显示）
    int total_width = (hour_num_w * HOUR_SIZE * 2) + colon_w + 
                     (min_num_w * MINUTE_SIZE * 2) + colon_w + 
                     (sec_num_w * SECOND_SIZE * 2) + dot_w + 
                     (tenth_num_w * TENTH_SIZE) + 40;
    int start_x = (menuSprite.width() - total_width) / 2;
    int y_main = (menuSprite.height() - (hour_num_h * HOUR_SIZE)) / 2;

    // 初始化tickers（只在第一次运行时）
    if (firstRun) {
        // 小时数字
        tickers[0] = {start_x, y_main, hour_num_w * HOUR_SIZE, hour_num_h * HOUR_SIZE, 
                      g_watchface_timeDate.hour / 10, 2, false, 0, HOUR_FONT, HOUR_SIZE};
        tickers[1] = {start_x + hour_num_w * HOUR_SIZE + 5, y_main, hour_num_w * HOUR_SIZE, hour_num_h * HOUR_SIZE, 
                      g_watchface_timeDate.hour % 10, 9, false, 0, HOUR_FONT, HOUR_SIZE};
        
        // 小时和分钟之间的冒号位置
        int colon1_x = start_x + hour_num_w * HOUR_SIZE * 2 + 10;
        
        // 分钟数字（在第一个冒号后）
        int minute_x = colon1_x + colon_w + 10;
        tickers[2] = {minute_x, y_main, min_num_w * MINUTE_SIZE, min_num_h * MINUTE_SIZE, 
                      g_watchface_timeDate.mins / 10, 5, false, 0, MINUTE_FONT, MINUTE_SIZE};
        tickers[3] = {minute_x + min_num_w * MINUTE_SIZE + 5, y_main, min_num_w * MINUTE_SIZE, min_num_h * MINUTE_SIZE, 
                      g_watchface_timeDate.mins % 10, 9, false, 0, MINUTE_FONT, MINUTE_SIZE};
        
        // 分钟和秒之间的冒号位置
        int colon2_x = minute_x + min_num_w * MINUTE_SIZE * 2 + 10;
        
        // 秒数字（在第二个冒号后）
        int sec_x = colon2_x + colon_w + 10;
        int sec_y = y_main + (hour_num_h * HOUR_SIZE - sec_num_h * SECOND_SIZE) / 2;
        tickers[4] = {sec_x, sec_y, sec_num_w * SECOND_SIZE, sec_num_h * SECOND_SIZE, 
                      g_watchface_timeDate.secs / 10, 5, false, 0, SECOND_FONT, SECOND_SIZE};
        tickers[5] = {sec_x + sec_num_w * SECOND_SIZE, sec_y, sec_num_w * SECOND_SIZE, sec_num_h * SECOND_SIZE, 
                      g_watchface_timeDate.secs % 10, 9, false, 0, SECOND_FONT, SECOND_SIZE};
        
        // 秒和0.1秒之间的点位置
        int dot_x = sec_x + sec_num_w * SECOND_SIZE * 2 + 5;
        
        // 0.1秒数字（在点后）
        int tenth_x = dot_x + dot_w + 5;
        int tenth_y = sec_y + (sec_num_h * SECOND_SIZE - tenth_num_h * TENTH_SIZE) / 2;
        tickers[6] = {tenth_x, tenth_y, tenth_num_w * TENTH_SIZE, tenth_num_h * TENTH_SIZE, 
                      g_watchface_timeDate.tenth, 9, false, 0, TENTH_FONT, TENTH_SIZE};
        
        firstRun = false;
    }

    // 检查时间变化，设置滚动标志
    if (g_watchface_timeDate.hour / 10 != last_time.hour / 10) { 
        tickers[0].moving = true; tickers[0].y_pos = 0; tickers[0].val = g_watchface_timeDate.hour / 10; 
    }
    if (g_watchface_timeDate.hour % 10 != last_time.hour % 10) { 
        tickers[1].moving = true; tickers[1].y_pos = 0; tickers[1].val = g_watchface_timeDate.hour % 10; 
    }
    if (g_watchface_timeDate.mins / 10 != last_time.mins / 10) { 
        tickers[2].moving = true; tickers[2].y_pos = 0; tickers[2].val = g_watchface_timeDate.mins / 10; 
    }
    if (g_watchface_timeDate.mins % 10 != last_time.mins % 10) { 
        tickers[3].moving = true; tickers[3].y_pos = 0; tickers[3].val = g_watchface_timeDate.mins % 10; 
    }
    if (g_watchface_timeDate.secs / 10 != last_time.secs / 10) { 
        tickers[4].moving = true; tickers[4].y_pos = 0; tickers[4].val = g_watchface_timeDate.secs / 10; 
    }
    if (g_watchface_timeDate.secs % 10 != last_time.secs % 10) { 
        tickers[5].moving = true; tickers[5].y_pos = 0; tickers[5].val = g_watchface_timeDate.secs % 10; 
    }
    if (g_watchface_timeDate.tenth != last_time.tenth) { 
        tickers[6].moving = true; tickers[6].y_pos = 0; tickers[6].val = g_watchface_timeDate.tenth; 
    }
    
    last_time = g_watchface_timeDate;

    menuSprite.fillSprite(TFT_BLACK);
    drawCommonElements();
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.setTextColor(TIME_MAIN_COLOR, TFT_BLACK);

    // 更新和绘制tickers
    for (int i = 0; i < 7; ++i) {
        if (tickers[i].moving) {
            int16_t* yPos = &tickers[i].y_pos;
            int current_h = tickers[i].h;
            
            if(*yPos <= 3) (*yPos)++;
            else if(*yPos <= 6) (*yPos) += 3;
            else if(*yPos <= 16) (*yPos) += 5;
            else if(*yPos <= 22) (*yPos) += 3;
            else if(*yPos <= current_h + TICKER_GAP) (*yPos)++;
            
            if (*yPos > current_h + TICKER_GAP) {
                tickers[i].moving = false;
                tickers[i].y_pos = 0;
            }
        }
        drawVectorScrollTickerNum(&tickers[i]);
    }

    // 绘制冒号和点
    menuSprite.setTextFont(COLON_FONT);
    menuSprite.setTextSize(COLON_SIZE);
    menuSprite.setTextColor(TIME_MAIN_COLOR, TFT_BLACK);
    
    // 计算冒号和点的位置
    int colon1_x = tickers[1].x + tickers[1].w + 5; // 小时和分钟之间的冒号
    int colon2_x = tickers[3].x + tickers[3].w + 5; // 分钟和秒之间的冒号
    int dot_x = tickers[5].x + tickers[5].w + 5;    // 秒和0.1秒之间的点
    
    int colon_y = y_main + (hour_num_h * HOUR_SIZE - menuSprite.fontHeight()) / 2;
    
    // 绘制小时和分钟之间的冒号
    menuSprite.drawString(":", colon1_x, colon_y);
    
    // 绘制分钟和秒之间的冒号
    menuSprite.drawString(":", colon2_x, colon_y);
    
    // 绘制秒和0.1秒之间的点
    int dot_y = tickers[5].y + (tickers[5].h - menuSprite.fontHeight()) / 2;
    menuSprite.drawString(".", dot_x, dot_y);

    menuSprite.pushSprite(0, 0);
}