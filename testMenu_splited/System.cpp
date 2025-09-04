#include <Arduino.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <driver/adc.h>
#include <esp_system.h>
#include "Alarm.h"
#include "WiFiManager.h"
#include "Menu.h"
#include "LED.h"
#include "Buzzer.h"
#include "weather.h"
#include "performance.h"
#include "DS18B20.h"
#include "System.h"
#include "ADC.h"
#include "img.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
extern DallasTemperature sensors;
extern OneWire oneWire;
extern Adafruit_NeoPixel strip;


TFT_eSPI tft = TFT_eSPI();
TFT_eSprite menuSprite = TFT_eSprite(&tft);

int tft_log_y = 40;
int current_log_lines = 0;
void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
  for(int i=0; i<NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}
// 打字机效果函数
void typeWriterEffect(const char* text, int x, int y, uint32_t color = TFT_WHITE, int delayMs = 30, bool playSound = false) {
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(1);
    
    for(int i = 0; text[i] != '\0'; i++) {
        tft.drawChar(text[i], x + i * 6, y);
        if (playSound) {
            tone(BUZZER_PIN, 800 + (i % 5) * 100, 10);
        }
        delay(delayMs);
    }
}

// 带命令行提示符的打字机效果
void typeWriterCommand(const char* text, int x, int y, uint32_t color = TFT_WHITE, int delayMs = 30) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(x, y);
    tft.print("> ");
    
    typeWriterEffect(text, x + 12, y, color, delayMs, true);
}

// 显示系统信息（打字机效果）
void displaySystemInfoTypewriter() {
    // 芯片信息
    #ifdef CONFIG_IDF_TARGET_ESP32
    tftLogInfo("Chip: ESP32");
    #elif CONFIG_IDF_TARGET_ESP32S2
    tftLogInfo("Chip: ESP32-S2");
    #elif CONFIG_IDF_TARGET_ESP32S3
    tftLogInfo("Chip: ESP32-S3");
    #elif CONFIG_IDF_TARGET_ESP32C3
    tftLogInfo("Chip: ESP32-C3");
    #else
    tftLogInfo("Chip: Unknown ESP");
    #endif

    // 核心数
    char coresInfo[20];
    #ifdef CONFIG_IDF_TARGET_ESP32
    sprintf(coresInfo, "Cores: %d", 2);
    #elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3)
    sprintf(coresInfo, "Cores: %d", 1);
    #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    sprintf(coresInfo, "Cores: %d", 2);
    #else
    sprintf(coresInfo, "Cores: %d", 1);
    #endif
    tftLogInfo(coresInfo);

    // CPU频率
    char cpuInfo[25];
    sprintf(cpuInfo, "CPU: %dMHz", ESP.getCpuFreqMHz());
    tftLogInfo(cpuInfo);

    // 闪存信息
    char flashInfo[35];
    sprintf(flashInfo, "Flash: %dMB", ESP.getFlashChipSize() / (1024 * 1024));
    tftLogInfo(flashInfo);

    // 内存信息
    char heapInfo[30];
    sprintf(heapInfo, "Heap: %d bytes", ESP.getFreeHeap());
    tftLogInfo(heapInfo);

    // SDK版本
    char sdkInfo[25];
    sprintf(sdkInfo, "SDK: %s", ESP.getSdkVersion());
    tftLogInfo(sdkInfo);
}

// 硬件测试函数（打字机效果）
void hardwareTest() {
   
    // 蜂鸣器测试
    tftLogInfo("Testing Buzzer...");
    tone(BUZZER_PIN, 1500, 200);
    delay(300);
    tftLogSuccess("Buzzer: OK");

    // 温度传感器测试
    tftLogInfo("Testing DS18B20...");
    sensors.begin();
    int deviceCount = sensors.getDeviceCount();
    char deviceStr[40];
    sprintf(deviceStr, "Found %d DS18B20 sensor(s)", deviceCount);
    tftLogSuccess(deviceStr);

    if (deviceCount == 0) {
        tftLogError("No DS18B20 sensors detected");
        return;
    }

    // 获取传感器分辨率
    uint8_t resolution = sensors.getResolution();
    char resStr[30];
    sprintf(resStr, "Resolution: %d bits", resolution);
    tftLogInfo(resStr);
    sensors.requestTemperatures();
    delay(1000);
    float tempC = sensors.getTempCByIndex(0);
    if (tempC != DEVICE_DISCONNECTED_C) {
        char tempStr[30];
        sprintf(tempStr, "Temp: %.1fC OK", tempC);
        tftLogInfo(tempStr);
    } else {
        tftLogError("DS18B20: FAILED");
    }

    DeviceAddress deviceAddress;
    if (sensors.getAddress(deviceAddress, 0)) {
        char addrStr[50];
        sprintf(addrStr, "Address: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                deviceAddress[0], deviceAddress[1], deviceAddress[2], deviceAddress[3],
                deviceAddress[4], deviceAddress[5], deviceAddress[6], deviceAddress[7]);
        tftLogInfo(addrStr);
    } else {
        tftLogError("FAILED to read sensor address");
    }
    // LED测试
    tftLogInfo("Testing LEDs...");
    setLEDColor(255, 0, 0); delay(200);
    setLEDColor(0, 255, 0); delay(200);
    setLEDColor(0, 0, 255); delay(200);
    setLEDColor(0, 0, 0);
    tftLogSuccess("LEDs: OK");

    // ADC测试
    tftLogInfo("Testing ADC...");
    uint32_t adcRaw = adc1_get_raw(ADC1_CHANNEL_2);
    char adcStr[25];
    sprintf(adcStr, "ADC: %lu OK", adcRaw);
    tftLogSuccess(adcStr);
}

// 开机动画函数
void bootAnimation() {
    static int boot_song_index = 14-2; // "Windows XP"
    xTaskCreatePinnedToCore(Buzzer_PlayMusic_Task, "BootSound", 8192, &boot_song_index, 1, NULL, 0);
    // tft.fillScreen(TFT_BLACK);
    const uint16_t* boot_gif[16] = {huaji_0,huaji_1,huaji_2,huaji_3,huaji_4,huaji_5,huaji_6,huaji_7,huaji_8,huaji_9,huaji_10,huaji_11,huaji_12,huaji_13,huaji_14,huaji_15};
    for(int i = 0;i<16;i++)
    {
        menuSprite.fillScreen(TFT_WHITE);
        menuSprite.pushImage(46,45,150,148,boot_gif[i]);
        menuSprite.pushSprite(0, 0);
        delay(50);
    }
    for(int i = 0;i<16;i++)
    {
        menuSprite.fillScreen(TFT_WHITE);
        menuSprite.pushImage(46,45,150,148,boot_gif[i]);
        menuSprite.pushSprite(0, 0);
        delay(50);
    }
    for(int i = 0;i<16;i++)
    {
        menuSprite.fillScreen(TFT_WHITE);
        menuSprite.pushImage(46,45,150,148,boot_gif[i]);
        menuSprite.pushSprite(0, 0);
        delay(50);
    }
    tft.fillScreen(TFT_BLACK);
    typeWriterEffect("SYSTEM BOOT SEQUENCE", 20, 5, TFT_GREEN, 50);
    tft.drawFastHLine(0, 25, SCREEN_WIDTH, TFT_DARKGREEN);
    
    // 显示系统信息
    displaySystemInfoTypewriter();
    delay(1000);
    
    // 清屏进行硬件测试
    tftClearLog();
    tftLogInfo("HARDWARE TEST");
    tft.drawFastHLine(0, 55, SCREEN_WIDTH, TFT_DARKCYAN);
    
    // 硬件测试
    hardwareTest();
    delay(1000);
    
    // 完成提示
    tft.fillRect(0, 120, SCREEN_WIDTH, LINE_HEIGHT * 2, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("All systems operational",120,80);
    tft.drawString("Start Connecting to WiFi...",120,160);
    tft.setTextFont(1);
    tft.setTextSize(1);
    delay(1500);
}

// 系统初始化函数
void bootSystem() {
    // 初始化硬件
    Buzzer_Init();
    initRotaryEncoder();
    DS18B20_Init();
    createDS18B20Task();
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);

    setupADC();
    menuSprite.createSprite(239, 239);


    // 运行开机动画
    bootAnimation();
    Alarm_Init(); // Initialize Alarm module and start background task

    // Connect to WiFi using the new manager
    // If it fails, it will start a config portal and loop forever until configured.
    connectWiFi_with_Manager();
    // 清屏进入主菜单
    tft.fillScreen(TFT_BLACK);
    synced = false;
    syncTime();
    showMenuConfig();
}



// 快速打字机效果函数
void fastTypeWriter(const char* text, int x, int y, uint32_t color = TFT_WHITE, int delayMs = 10) {
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(1);
    
    for(int i = 0; text[i] != '\0'; i++) {
        tft.drawChar(text[i], x + i * 6, y);
        delay(delayMs);
    }
}

// 清空日志区域
void tftClearLog() {
    tft.fillRect(0, 30, tft.width(), tft.height() - 30, TFT_BLACK);
    tft_log_y = 40;
    current_log_lines = 0;
}

// 显示日志标题栏（快速打字机效果）
void tftLogHeader(const String& title) {
    // 绘制标题栏背景
    tft.fillRect(0, 0, tft.width(), 30, TFT_DARKGREEN);
    
    // 绘制标题（快速打字机效果）
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM);
    
    // 使用快速打字机效果显示标题
    int titleX = (tft.width() - title.length() * 12) / 2;
    for(int i = 0; i < title.length(); i++) {
        tft.drawChar(title[i], titleX + i * 12, 10);
        delay(20); // 快速显示
    }
    
    // 绘制分隔线
    tft.drawFastHLine(0, 30, tft.width(), TFT_GREEN);
    
    // 重置日志位置
    tftClearLog();
    
    // 恢复默认文本设置
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
}

// 快速打字机日志函数（无时间戳）
void tftLog(String text, uint16_t color) {
    tft.setTextFont(1);
    tft.setTextSize(1);
    // 检查是否需要清屏滚动
    if (current_log_lines >= LOG_MAX_LINES) {
        tftClearLog();
    }
    
    // 清除当前行区域
    tft.fillRect(LOG_MARGIN, tft_log_y, tft.width() - LOG_MARGIN * 2, LOG_LINE_HEIGHT, TFT_BLACK);
    
    // 组合提示符和文本
    String fullText = "> " + text;
    
    // 使用typeWriterEffect显示完整内容
    typeWriterEffect(fullText.c_str(), LOG_MARGIN, tft_log_y, color, 8, true);
    
    tft_log_y += LOG_LINE_HEIGHT;
    current_log_lines++;
}

// 不同级别的日志函数（快速打字机效果）
void tftLogInfo(const String& text) {
    tftLog(text, TFT_CYAN);
}

void tftLogWarning(const String& text) {
    tftLog(text, TFT_YELLOW);
}

void tftLogError(const String& text) {
    tftLog(text, TFT_RED);
}

void tftLogSuccess(const String& text) {
    tftLog(text, TFT_GREEN);
}

void tftLogDebug(const String& text) {
    tftLog(text, TFT_MAGENTA);
}

// 显示带图标的日志（快速打字机效果）
void tftLogWithIcon(const String& text, uint16_t color, const char* icon) {
    String iconText = String(icon) + " " + text;
    tftLog(iconText, color);
}

// 进度条样式日志（快速打字机效果）
void tftLogProgress(const String& text, int progress, int total) {
    if (progress == 0) {
        tftLog(text + "...", TFT_WHITE);
    } else if (progress == total) {
        tftLog(text + " ✓", TFT_GREEN);
    } else {
        char progressStr[20];
        int percent = (progress * 100) / total;
        snprintf(progressStr, sizeof(progressStr), " (%d%%)", percent);
        tftLog(text + progressStr, TFT_WHITE);
    }
}

// 超快速日志函数（几乎实时显示）
void tftLogFast(const String& text, uint16_t color = TFT_GREEN) {
    // 检查是否需要清屏滚动
    if (current_log_lines >= LOG_MAX_LINES) {
        tftClearLog();
    }
    
    // 清除当前行区域
    tft.fillRect(LOG_MARGIN, tft_log_y, tft.width() - LOG_MARGIN * 2, LOG_LINE_HEIGHT, TFT_BLACK);
    
    // 显示命令行提示符和文本（直接显示，无打字效果）
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(LOG_MARGIN, tft_log_y);
    tft.print("> ");
    
    tft.setTextColor(color, TFT_BLACK);
    tft.print(text);
    
    tft_log_y += LOG_LINE_HEIGHT;
    current_log_lines++;
}

// 批量快速显示多条日志
void tftLogMultiple(const String texts[], uint16_t colors[], int count) {
    for(int i = 0; i < count; i++) {
        tftLog(texts[i], colors[i]);
        delay(50); // 短暂延迟使效果更自然
    }
}