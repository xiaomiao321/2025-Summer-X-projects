#include <TFT_eSPI.h>
#include <cmath>
#include "Buzzer.h"
#include <vector>
#include "Watchface.h"
#include "RotaryEncoder.h"
#include "weather.h" 
#include "img.h"
#include <time.h> // For struct tm

// =================================================================================================
// Forward Declarations & Menu Setup
// =================================================================================================
extern TFT_eSPI tft;
extern TFT_eSprite menuSprite;
extern void showMenuConfig();

// Forward declare all watchface functions
static void SimpleClockWatchface();
static void VectorScanWatchface();
static void VectorScrollWatchface();
static void TerminalSimWatchface();
static void CodeRainWatchface();
static void SnowWatchface();
static void WavesWatchface();
static void NenoWatchface();
static void BallsWatchface();
static void SandBoxWatchface();
static void ProgressBarWatchface(); // New watchface
static void Cube3DWatchface();
static void GalaxyWatchface();
static void SimClockWatchface();
static void PlaceholderWatchface();

struct WatchfaceItem {
    const char *name;
    void (*show)();
};

const WatchfaceItem watchfaceItems[] = {
    {"Vector Scan", VectorScanWatchface},
    {"Simple Clock", SimpleClockWatchface},
    {"Sim Clock", SimClockWatchface},
    {"Terminal Sim", TerminalSimWatchface},
    {"Code Rain", CodeRainWatchface},
    {"Snow", SnowWatchface},
    {"Waves", WavesWatchface},
    {"Neno", NenoWatchface},
    {"Bouncing Balls", BallsWatchface},
    {"Sand Box", SandBoxWatchface},
    {"Progress Bar", ProgressBarWatchface},
    {"Vector Scroll", VectorScrollWatchface},
    {"3D Cube", Cube3DWatchface},
    {"Galaxy", GalaxyWatchface},
};
const int WATCHFACE_COUNT = sizeof(watchfaceItems) / sizeof(watchfaceItems[0]);

// --- Pagination for Watchface Menu ---
const int VISIBLE_WATCHFACES = 5; // Number of watchfaces to display at once

static void displayWatchfaceList(int selectedIndex, int displayOffset) {
    menuSprite.fillSprite(TFT_BLACK);
    menuSprite.setTextDatum(MC_DATUM);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.setTextSize(2);
    menuSprite.drawString("Select Watchface", tft.width() / 2, 20);

    for (int i = 0; i < VISIBLE_WATCHFACES; i++) {
        int itemIndex = displayOffset + i;
        if (itemIndex >= WATCHFACE_COUNT) break;

        menuSprite.setTextSize(itemIndex == selectedIndex ? 2 : 1);
        menuSprite.setTextColor(itemIndex == selectedIndex ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        menuSprite.drawString(watchfaceItems[itemIndex].name, tft.width() / 2, 60 + i * 30);
    }
    menuSprite.pushSprite(0, 0);
}

void WatchfaceMenu() {
    int selectedIndex = 0;
    int displayOffset = 0;
    unsigned long lastClickTime = 0;
    bool singleClick = false;

    displayWatchfaceList(selectedIndex, displayOffset);

    while(1) {
        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            selectedIndex = (selectedIndex + encoderChange + WATCHFACE_COUNT) % WATCHFACE_COUNT;
            
            if (selectedIndex < displayOffset) {
                displayOffset = selectedIndex;
            } else if (selectedIndex >= displayOffset + VISIBLE_WATCHFACES) {
                displayOffset = selectedIndex - VISIBLE_WATCHFACES + 1;
            }
            displayWatchfaceList(selectedIndex, displayOffset);
            tone(BUZZER_PIN, 1000, 50);
        }

        if (readButton()) {
            if (millis() - lastClickTime < 300) { showMenuConfig(); return; }
            lastClickTime = millis();
            singleClick = true;
        }

        if (singleClick && (millis() - lastClickTime > 300)) {
            singleClick = false;
            tone(BUZZER_PIN, 2000, 50);
            watchfaceItems[selectedIndex].show();
            displayWatchfaceList(selectedIndex, displayOffset);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// =================================================================================================
// API ADAPTER LAYER & HELPERS
// =================================================================================================

extern struct tm timeinfo;
typedef uint8_t byte;
typedef uint32_t millis_t;
typedef struct { byte hour, mins, secs; char ampm; } time_s;
typedef struct { time_s time; byte day, month, year; } timeDate_s;
timeDate_s g_watchface_timeDate;
timeDate_s *timeDate = &g_watchface_timeDate;

static uint32_t util_seed = 1;
void util_randomSeed(uint32_t newSeed) { util_seed = newSeed; }
long util_random(long max) { util_seed = (util_seed * 1103515245 + 12345) & 0x7FFFFFFF; return util_seed % max; }
long util_random_range(long min, long max) { return min + util_random(max - min); }

void draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    menuSprite.setTextSize(size);
    menuSprite.setTextColor(color, bg);
    char str[2] = {c, '\0'};
    menuSprite.drawString(str, x, y);
}

// =================================================================================================
// Hourly Chime Logic
// =================================================================================================
static int g_lastChimeSecond = -1;
static TaskHandle_t g_hourlyMusicTaskHandle = NULL;
static int g_hourlySongIndex = 0;

static void handleHourlyChime() {
    static unsigned long lastChimeCheck = 0;

    // This function will be called frequently from watchface loops.
    // We only run the logic once per second.
    if (millis() - lastChimeCheck < 1000) {
        return;
    }
    lastChimeCheck = millis();

    getLocalTime(&timeinfo);

    // Check if the music task has finished on its own
    if (g_hourlyMusicTaskHandle != NULL && eTaskGetState(g_hourlyMusicTaskHandle) == eDeleted) {
        g_hourlyMusicTaskHandle = NULL;
    }

    // Sequence from 59:55 to 59:59
    if (timeinfo.tm_min == 59 && timeinfo.tm_sec >= 55) {
        if (g_lastChimeSecond != timeinfo.tm_sec) {
            int freq = 1000 + (timeinfo.tm_sec - 55) * 200; // Increasing pitch
            tone(BUZZER_PIN, freq, 100);
            g_lastChimeSecond = timeinfo.tm_sec;
        }
    } 
    // At 00:00, play music
    else if (timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        if (g_lastChimeSecond == 59) { // Trigger only once when coming from 59:59
            tone(BUZZER_PIN, 2500, 150); // Final, highest pitch beep

            // If a song is already playing, stop it before starting a new one
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
            }

            // Start a random song
            g_hourlySongIndex = util_random(numSongs);
            stopBuzzerTask = false; // Reset stop flag
            xTaskCreatePinnedToCore(Buzzer_PlayMusic_Task, "Buzzer_Music_Task", 8192, &g_hourlySongIndex, 1, &g_hourlyMusicTaskHandle, 0);
        }
        g_lastChimeSecond = timeinfo.tm_sec;
    } else {
        // Reset for next hour
        g_lastChimeSecond = -1;
    }
}

// =================================================================================================
// Common Watchface Logic
// =================================================================================================

static unsigned long lastSyncMillis = 0;
const unsigned long syncInterval = 5 * 60 * 1000; // 5 minutes

static void handlePeriodicSync() {
    if (millis() - lastSyncMillis > syncInterval) {
        if(wifi_connected) {
            silentSyncTime();
            silentFetchWeather();
        }
        lastSyncMillis = millis();
    }
}

static void drawCommonElements() {
    // Weather (top right)
    menuSprite.setTextDatum(TR_DATUM);
    menuSprite.setTextSize(2);
    menuSprite.setTextColor(TFT_ORANGE, TFT_BLACK);
    String weatherStr = String(temperature) + " " + String(humidity);
    menuSprite.drawString(weatherStr, tft.width() - 15, 5);
    menuSprite.setTextSize(1);
    menuSprite.setTextDatum(TL_DATUM);
    menuSprite.drawString(reporttime,5,5);

    // Date & Day of Week (Centered, two lines)
    menuSprite.setTextDatum(MC_DATUM);
    const char* weekDayStr[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    
    // Line 1: Weekday
    menuSprite.setTextSize(3);
    menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    menuSprite.drawString(weekDayStr[timeinfo.tm_wday], tft.width() / 2, 45); // Y=45

    // Line 2: YYYY-MM-DD
    char dateStr[20];
    sprintf(dateStr, "%d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    menuSprite.setTextSize(3);
    menuSprite.drawString(dateStr, tft.width() / 2, 75); // Y=75

    // Last Sync Time (bottom center)
    menuSprite.setTextDatum(BC_DATUM);
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_CYAN, TFT_BLACK);
    menuSprite.drawString(lastSyncTimeStr, 120, tft.height() - 15);

    // Last Sync Weather
    menuSprite.setTextDatum(BC_DATUM);
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    menuSprite.drawString(lastWeatherSyncStr, 120, tft.height()-25);
}

// =================================================================================================
// WATCHFACE IMPLEMENTATIONS
// =================================================================================================

// --- 3D Cube ---
#define CUBE_SIZE 25

static const int16_t cube_vertices_start[] = {
    -CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE,
    CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE,
    CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE,
    -CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE,
    -CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE,
    CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE,
    CUBE_SIZE, CUBE_SIZE, CUBE_SIZE,
    -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE
};

static const uint8_t cube_indices[] = {
    0, 1, 1, 2, 2, 3, 3, 0,
    4, 5, 5, 6, 6, 7, 7, 4,
    0, 4, 1, 5, 2, 6, 3, 7
};

static void Cube3DWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    static float rot = 0;
    static float rotInc = 1;
    int16_t cube_vertices[8 * 3];

    while(1) {
        handlePeriodicSync();
        handleHourlyChime();

        int encoderChange = readEncoder();
        if (encoderChange != 0) {
            rotInc += encoderChange;
            if (rotInc > 10) rotInc = 10;
            if (rotInc < 1) rotInc = 1;
            tone(BUZZER_PIN, 1000, 50);
        }

        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);
        
        memcpy(cube_vertices, cube_vertices_start, sizeof(cube_vertices_start));

        rot += rotInc / 10.0;
        if(rot >= 360) rot = 0;
        else if(rot < 0) rot = 359;

        float rot_rad = rot * M_PI / 180.0;
        float rot_rad2 = (rot * 1.5) * M_PI / 180.0; // Adjusted rotation speed for different axes
        float rot_rad3 = (rot * 2.0) * M_PI / 180.0;

        float sin_rot = sin(rot_rad);
        float cos_rot = cos(rot_rad);
        float sin_rot2 = sin(rot_rad2);
        float cos_rot2 = cos(rot_rad2);
        float sin_rot3 = sin(rot_rad3);
        float cos_rot3 = cos(rot_rad3);

        for(int i=0; i<8; ++i) {
            int16_t* vertex = &cube_vertices[i * 3];
            int16_t x = vertex[0], y = vertex[1], z = vertex[2];

            int16_t x_new = x * cos_rot - y * sin_rot;
            int16_t y_new = y * cos_rot + x * sin_rot;
            x = x_new; y = y_new;

            x_new = x * cos_rot2 - z * sin_rot2;
            z = z * cos_rot2 + x * sin_rot2;
            x = x_new;

            y_new = y * cos_rot3 - z * sin_rot3;
            z = z * cos_rot3 + y * sin_rot3;
            y = y_new;

            int16_t z_new = z + 80;
            x = (x * 128) / z_new;
            y = (y * 128) / z_new;

            x += tft.width() / 2;
            y += tft.height() / 2;

            vertex[0] = x;
            vertex[1] = y;
        }

        for(int i=0; i<12; ++i) {
            const uint8_t* indices = &cube_indices[i * 2];
            int16_t* p1 = &cube_vertices[indices[0] * 3];
            int16_t* p2 = &cube_vertices[indices[1] * 3];
            menuSprite.drawLine(p1[0], p1[1], p2[0], p2[1], TFT_WHITE);
        }

        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        menuSprite.setTextDatum(TC_DATUM);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.drawString(timeStr, tft.width()/2, 5);

        char secStr[5];
        int tenth = (millis() % 1000) / 100;
        sprintf(secStr, "%02d.%d", timeinfo.tm_sec, tenth);
        menuSprite.setTextDatum(BC_DATUM);
        menuSprite.setTextSize(3);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.drawString(secStr, tft.width()/2, tft.height() - 5);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Galaxy ---
struct GalaxyStar {
    int x;
    int y;
    int time;
};

static GalaxyStar galaxy_stars[50];

static float galaxy_radians(float angle) {
    return angle * M_PI / 180.0;
}

static void galaxy_draw_spiral_stars(uint16_t num_stars, uint16_t arm_length, uint16_t spread, uint16_t rotation_offset) {
    for (int i = 0; i < num_stars; i++) {
        int angle = i * spread + rotation_offset;
        int length = arm_length * i / num_stars;
        
        int x = length * cos(galaxy_radians(angle)) * 3 / 5;
        int y = length * sin(galaxy_radians(angle)) * 3 / 5;
        
        menuSprite.drawPixel(x + tft.width() / 2, y + tft.height() / 2, TFT_WHITE);
    }
}

static void galaxy_draw_main() {
    static uint16_t rotation_angle = 0;
    for (int i = 0; i < 4; i++) {
        galaxy_draw_spiral_stars(15, 130, 10, i * 90 + rotation_angle);
    }
    if (++rotation_angle >= 360) {
        rotation_angle = 0;
    }
}

static void galaxy_random_drawStars() {
    static uint32_t time_counter = 0;
    if (time_counter % 2 == 0) {
        int i = 0;
        while (galaxy_stars[i].time && i < 50) {
            i++;
        }
        if (i < 50) {
            galaxy_stars[i].time = 30;
            galaxy_stars[i].x = util_random(tft.width());
            galaxy_stars[i].y = util_random(tft.height());
        }
    }

    for (int i = 0; i < 50; i++) {
        if (galaxy_stars[i].time > 0) {
            galaxy_stars[i].time--;
            menuSprite.drawPixel(galaxy_stars[i].x, galaxy_stars[i].y, TFT_WHITE);
        }
    }
    time_counter++;
}

static void GalaxyWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    for(int i=0; i<50; ++i) {
        galaxy_stars[i].time = 0;
    }

    while(1) {
        handlePeriodicSync();
        handleHourlyChime();

        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);
        
        galaxy_draw_main();
        galaxy_random_drawStars();

        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        menuSprite.setTextDatum(TC_DATUM);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.drawString(timeStr, tft.width()/2, 5);

        char secStr[5];
        int tenth = (millis() % 1000) / 100;
        sprintf(secStr, "%02d.%d", timeinfo.tm_sec, tenth);
        menuSprite.setTextDatum(BC_DATUM);
        menuSprite.setTextSize(3);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.drawString(secStr, tft.width()/2, tft.height() - 5);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void SimClockWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;

    while(1) {
        handlePeriodicSync();
        handleHourlyChime();

        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);

        int centerX = tft.width() / 2;
        int centerY = tft.height() / 2;
        int radius = std::min(tft.width(), tft.height()) / 2 - 10; 

        // Draw clock circle
        menuSprite.drawCircle(centerX, centerY, radius, TFT_WHITE);

        // Draw center dot
        menuSprite.fillCircle(centerX, centerY, 3, TFT_WHITE);

        // Hour hand
        float hourAngle = (timeinfo.tm_hour % 12 + timeinfo.tm_min / 60.0) * 30 - 90; // -90 for 12 o'clock at top
        int hourX = centerX + (int)(0.5 * radius * cos(hourAngle * M_PI / 180.0));
        int hourY = centerY + (int)(0.5 * radius * sin(hourAngle * M_PI / 180.0));
        menuSprite.drawLine(centerX, centerY, hourX, hourY, TFT_RED);

        // Minute hand
        float minAngle = (timeinfo.tm_min + timeinfo.tm_sec / 60.0) * 6 - 90;
        int minX = centerX + (int)(0.8 * radius * cos(minAngle * M_PI / 180.0));
        int minY = centerY + (int)(0.8 * radius * sin(minAngle * M_PI / 180.0));
        menuSprite.drawLine(centerX, centerY, minX, minY, TFT_GREEN);

        // Second hand
        float secAngle = (timeinfo.tm_sec + (millis() % 1000) / 1000.0) * 6 - 90;
        int secX = centerX + (int)(0.9 * radius * cos(secAngle * M_PI / 180.0));
        int secY = centerY + (int)(0.9 * radius * sin(secAngle * M_PI / 180.0));
        menuSprite.drawLine(centerX, centerY, secX, secY, TFT_BLUE);

        // --- Draw Time ---
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        menuSprite.setTextDatum(TC_DATUM);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.drawString(timeStr, tft.width()/2, 5);

        char secStr[5];
        int tenth = (millis() % 1000) / 100;
        sprintf(secStr, "%02d.%d", timeinfo.tm_sec, tenth);
        menuSprite.setTextDatum(BC_DATUM);
        menuSprite.setTextSize(3);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.drawString(secStr, tft.width()/2, tft.height() - 5);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(50)); // Update frequently for smooth second hand
    }
}

static void PlaceholderWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        menuSprite.setTextSize(2);
        menuSprite.drawString("Placeholder", tft.width()/2, tft.height()/2 + 20);
        
        menuSprite.pushSprite(0,0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// --- Vector Scan ---
#define TICKER_GAP 4
struct TickerData {
    int16_t x, y, w, h;
    uint8_t val, max_val;
    bool moving;
    int16_t y_pos;
};

static void drawTickerNum(TickerData* data) {
    int16_t y_pos = data->y_pos;
    int16_t x = data->x, y = data->y, w = data->w, h = data->h;
    uint8_t prev_val = (data->val == 0) ? data->max_val : data->val - 1;
    char current_char[2] = {(char)('0' + data->val), '\0'};
    char prev_char[2] = {(char)('0' + prev_val), '\0'};

    if (!data->moving || y_pos == 0) {
        menuSprite.drawString(current_char, x, y);
        return;
    }

    int16_t reveal_height = y_pos;
    if (reveal_height > h) reveal_height = h;

    menuSprite.setViewport(x, y + h - reveal_height, w, reveal_height);
    menuSprite.drawString(current_char, 0, -(h - reveal_height));
    menuSprite.resetViewport();

    menuSprite.setViewport(x, y, w, h - reveal_height);
    menuSprite.drawString(prev_char, 0, 0);
    menuSprite.resetViewport();

    menuSprite.drawFastHLine(x, y + h - reveal_height, w, TFT_WHITE);
    menuSprite.drawLine(tft.width()/2, tft.height()-1, x + w/2, y + h - reveal_height, TFT_GREEN);
}

// --- Vector Scroll ---
static void drawVectorScrollTickerNum(TickerData* data) {
    int16_t y_pos = data->y_pos;
    int16_t x = data->x, y = data->y, h = data->h;
    uint8_t prev_val = (data->val == 0) ? data->max_val : data->val - 1;
    char current_char[2] = {(char)('0' + data->val), '\0'};
    char prev_char[2] = {(char)('0' + prev_val), '\0'};

    if (!data->moving || y_pos == 0 || y_pos > h + TICKER_GAP) {
        menuSprite.drawString(current_char, x, y);
        return;
    }

    // Draw the new number scrolling in from the bottom
    // It starts at y + h + TICKER_GAP and moves to y.
    int16_t new_y = y + h + TICKER_GAP - y_pos;
    menuSprite.drawString(current_char, x, new_y);

    // Draw the old number scrolling up and out
    // It starts at y and moves to y - (h + TICKER_GAP)
    int16_t old_y = y - y_pos;
    menuSprite.drawString(prev_char, x, old_y);
}

static void VectorScrollWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    time_s last_time = {255, 255, 255};
    TickerData tickers[6];
    
    int num_w = 35, num_h = 50;
    int colon_w = 15; // Width for colon spacing
    int start_x = (tft.width() - (num_w * 4 + colon_w * 2 + num_w * 2)) / 2; // Adjusted for 6 digits + 2 colons
    int y_main = (tft.height() - num_h) / 2;

    // Hour, Minute, Second tickers
    tickers[0] = {start_x, y_main, num_w, num_h, 0, 2, false, 0};
    tickers[1] = {start_x + num_w+5, y_main, num_w, num_h, 0, 9, false, 0};
    tickers[2] = {start_x + num_w*2 + colon_w + 20, y_main, num_w, num_h, 0, 5, false, 0}; // Shifted right
    tickers[3] = {start_x + num_w*3 + colon_w + 20+5, y_main, num_w, num_h, 0, 9, false, 0}; // Shifted right
    // Seconds are now smaller and below minutes
    int sec_num_w = 20, sec_num_h = 25;
    int sec_x_offset = tickers[3].x + num_w + 5; // Right of last minute digit, slightly back
    int sec_y_offset = y_main + num_h - sec_num_h + 5; // Below minutes

    tickers[4] = {sec_x_offset, sec_y_offset, sec_num_w, sec_num_h, 0, 5, false, 0};
    tickers[5] = {sec_x_offset + sec_num_w, sec_y_offset, sec_num_w, sec_num_h, 0, 9, false, 0};


    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        g_watchface_timeDate.time.hour = timeinfo.tm_hour;
        g_watchface_timeDate.time.mins = timeinfo.tm_min;
        g_watchface_timeDate.time.secs = timeinfo.tm_sec;

        if (g_watchface_timeDate.time.hour / 10 != last_time.hour / 10) { tickers[0].moving = true; tickers[0].y_pos = 0; tickers[0].val = g_watchface_timeDate.time.hour / 10; }
        if (g_watchface_timeDate.time.hour % 10 != last_time.hour % 10) { tickers[1].moving = true; tickers[1].y_pos = 0; tickers[1].val = g_watchface_timeDate.time.hour % 10; }
        if (g_watchface_timeDate.time.mins / 10 != last_time.mins / 10) { tickers[2].moving = true; tickers[2].y_pos = 0; tickers[2].val = g_watchface_timeDate.time.mins / 10; }
        if (g_watchface_timeDate.time.mins % 10 != last_time.mins % 10) { tickers[3].moving = true; tickers[3].y_pos = 0; tickers[3].val = g_watchface_timeDate.time.mins % 10; }
        if (g_watchface_timeDate.time.secs / 10 != last_time.secs / 10) { tickers[4].moving = true; tickers[4].y_pos = 0; tickers[4].val = g_watchface_timeDate.time.secs / 10; }
        if (g_watchface_timeDate.time.secs % 10 != last_time.secs % 10) { tickers[5].moving = true; tickers[5].y_pos = 0; tickers[5].val = g_watchface_timeDate.time.secs % 10; }
        last_time = g_watchface_timeDate.time;

        menuSprite.fillSprite(TFT_BLACK);
        drawCommonElements(); 
        menuSprite.setTextDatum(TL_DATUM);

        // Update and draw tickers
        for (int i=0; i<6; ++i) {
            bool is_sec = (i >= 4);
            int current_h = is_sec ? sec_num_h : num_h;
            menuSprite.setTextSize(is_sec ? 3 : 7); // smaller seconds
            
            if (tickers[i].moving) {
                int16_t* yPos = &tickers[i].y_pos;
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

        // Draw blinking colon between HH and MM
        if (millis() % 1000 < 500) {
            menuSprite.setTextSize(4); // Smaller colon
            menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            menuSprite.drawString(":", start_x + num_w*2 + 10, y_main + 5); // Adjusted position
        }

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextDatum(TL_DATUM); // Top-Left datum
        int x_pos = tickers[5].x + tickers[5].w;
        int y_pos = tickers[5].y + tickers[5].h - 8; // -8 for font height
        menuSprite.drawString(String(tenth), x_pos, y_pos);

        menuSprite.pushSprite(0,0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void VectorScanWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    time_s last_time = {255, 255, 255};
    TickerData tickers[6];
    int num_w = 35, num_h = 50;
    int colon_w = 15; // Width for colon spacing
    int start_x = (tft.width() - (num_w * 4 + colon_w * 2 + num_w * 2)) / 2; // Adjusted for 6 digits + 2 colons
    int y_main = (tft.height() - num_h) / 2;

    // Hour, Minute, Second tickers
    tickers[0] = {start_x, y_main, num_w, num_h, 0, 2, false, 0};
    tickers[1] = {start_x + num_w+5, y_main, num_w, num_h, 0, 9, false, 0};
    tickers[2] = {start_x + num_w*2 + colon_w + 20, y_main, num_w, num_h, 0, 5, false, 0}; // Shifted right
    tickers[3] = {start_x + num_w*3 + colon_w + 20+5, y_main, num_w, num_h, 0, 9, false, 0}; // Shifted right
    // Seconds are now smaller and below minutes
    int sec_num_w = 20, sec_num_h = 25;
    int sec_x_offset = tickers[3].x + num_w + 5; // Right of last minute digit, slightly back
    int sec_y_offset = y_main + num_h - sec_num_h + 5; // Below minutes

    tickers[4] = {sec_x_offset, sec_y_offset, sec_num_w, sec_num_h, 0, 5, false, 0};
    tickers[5] = {sec_x_offset + sec_num_w, sec_y_offset, sec_num_w, sec_num_h, 0, 9, false, 0};


    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        g_watchface_timeDate.time.hour = timeinfo.tm_hour;
        g_watchface_timeDate.time.mins = timeinfo.tm_min;
        g_watchface_timeDate.time.secs = timeinfo.tm_sec;

        if (g_watchface_timeDate.time.hour / 10 != last_time.hour / 10) { tickers[0].moving = true; tickers[0].y_pos = 0; tickers[0].val = g_watchface_timeDate.time.hour / 10; }
        if (g_watchface_timeDate.time.hour % 10 != last_time.hour % 10) { tickers[1].moving = true; tickers[1].y_pos = 0; tickers[1].val = g_watchface_timeDate.time.hour % 10; }
        if (g_watchface_timeDate.time.mins / 10 != last_time.mins / 10) { tickers[2].moving = true; tickers[2].y_pos = 0; tickers[2].val = g_watchface_timeDate.time.mins / 10; }
        if (g_watchface_timeDate.time.mins % 10 != last_time.mins % 10) { tickers[3].moving = true; tickers[3].y_pos = 0; tickers[3].val = g_watchface_timeDate.time.mins % 10; }
        if (g_watchface_timeDate.time.secs / 10 != last_time.secs / 10) { tickers[4].moving = true; tickers[4].y_pos = 0; tickers[4].val = g_watchface_timeDate.time.secs / 10; }
        if (g_watchface_timeDate.time.secs % 10 != last_time.secs % 10) { tickers[5].moving = true; tickers[5].y_pos = 0; tickers[5].val = g_watchface_timeDate.time.secs % 10; }
        last_time = g_watchface_timeDate.time;

        menuSprite.fillSprite(TFT_BLACK);
        drawCommonElements();
        menuSprite.setTextDatum(TL_DATUM);

  // Update and draw tickers
        for (int i=0; i<6; ++i) {
            menuSprite.setTextSize((i < 4) ? 7 : 3); // HH:MM large, SS smaller
            if (tickers[i].moving) {
                tickers[i].y_pos += 5;
                if (tickers[i].y_pos >= ((i < 4) ? num_h : sec_num_h) + TICKER_GAP) tickers[i].moving = false;
            }
            drawTickerNum(&tickers[i]);
        }

        // Draw blinking colon between HH and MM
        if (millis() % 1000 < 500) {
            menuSprite.setTextSize(4); // Smaller colon
            menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            menuSprite.drawString(":", start_x + num_w*2 + 10, y_main + 5); // Adjusted position
        }

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextDatum(TL_DATUM); // Make sure datum is correct
        int x_pos = tickers[5].x + tickers[5].w;
        int y_pos = tickers[5].y + tickers[5].h - 8;
        menuSprite.drawString(String(tenth), x_pos, y_pos);

        menuSprite.pushSprite(0,0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Simple Clock ---
static void SimpleClockWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();

        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }
        
        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(5);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = tft.height()/2 + 20;
        menuSprite.drawString(timeStr, timeX, timeY);

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// --- Terminal Sim & Code Rain ---
#define RAIN_COLS 30
char rain_chars[RAIN_COLS];
int rain_pos[RAIN_COLS];
int rain_speed[RAIN_COLS];

static void shared_rain_logic(uint16_t color) {
    lastSyncMillis = millis() - syncInterval - 1;
    util_randomSeed(millis());
    for(int i=0; i<RAIN_COLS; i++) {
        rain_pos[i] = util_random_range(0, tft.height());
        rain_speed[i] = util_random_range(1, 5);
        rain_chars[i] = (char)util_random_range(33, 126);
    }
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);
        
        for(int i=0; i<RAIN_COLS; i++) {
            rain_pos[i] += rain_speed[i];
            if(rain_pos[i] > tft.height()) {
                rain_pos[i] = 0;
                rain_chars[i] = (char)util_random_range(33, 126);
            }
            draw_char(i * 8, rain_pos[i], rain_chars[i], color, TFT_BLACK, 1);
        }
        
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = tft.height()/2 + 20;
        menuSprite.drawString(timeStr, timeX, timeY);

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void TerminalSimWatchface() { shared_rain_logic(TFT_GREEN); }
static void CodeRainWatchface() { shared_rain_logic(TFT_CYAN); }

// --- Snow ---
#define SNOW_PARTICLES 100
std::vector<std::pair<int, int>> snow_particles(SNOW_PARTICLES);
static void SnowWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    util_randomSeed(millis());
    for(auto& p : snow_particles) { p.first = util_random_range(0, tft.width()); p.second = util_random_range(0, tft.height()); }
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);

        for(auto& p : snow_particles) {
            p.second += 1;
            if(p.second > tft.height()) { p.second = 0; p.first = util_random_range(0, tft.width()); }
            menuSprite.drawPixel(p.first, p.second, TFT_WHITE);
        }
        
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = tft.height()/2 + 20;
        menuSprite.drawString(timeStr, timeX, timeY);

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// --- Waves ---
static void WavesWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    float time = 0;
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);

        for(int x=0; x<tft.width(); x++) {
            menuSprite.drawPixel(x, sin((float)x/20+time)*20+60, TFT_CYAN);
            menuSprite.drawPixel(x, cos((float)x/15+time)*20+120, TFT_MAGENTA);
            menuSprite.drawPixel(x, sin((float)x/10+time*2)*20+180, TFT_YELLOW);
        }
        time += 0.1;
        
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = tft.height()/2 + 20;
        menuSprite.drawString(timeStr, timeX, timeY);

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Neno (Neon Lines) ---
static void NenoWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    float time = 0;
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);

        float x1 = tft.width()/2+sin(time)*50, y1 = tft.height()/2+cos(time)*50;
        float x2 = tft.width()/2+sin(time+PI)*50, y2 = tft.height()/2+cos(time+PI)*50;
        menuSprite.drawLine(x1, y1, x2, y2, TFT_RED);
        menuSprite.drawLine(tft.width()-x1, y1, tft.width()-x2, y2, TFT_BLUE);
        time += 0.05;
        
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = tft.height()/2 + 20;
        menuSprite.drawString(timeStr, timeX, timeY);

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Bouncing Balls ---
#define BALL_COUNT 5
struct Ball { float x, y, vx, vy; uint16_t color; };
std::vector<Ball> balls(BALL_COUNT);

static void BallsWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    util_randomSeed(millis());
    for(auto& b : balls) {
        b.x = util_random_range(10, tft.width()-10); b.y = util_random_range(10, tft.height()-10);
        b.vx = util_random_range(-3, 3); b.vy = util_random_range(-3, 3);
        b.color = util_random(0xFFFF);
    }
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);

        for(auto& b : balls) {
            b.x += b.vx; b.y += b.vy;
            if(b.x < 5 || b.x > tft.width()-5) b.vx *= -1;
            if(b.y < 5 || b.y > tft.height()-5) b.vy *= -1;
            menuSprite.fillCircle(b.x, b.y, 5, b.color);
        }
        
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = tft.height()/2 + 20;
        menuSprite.drawString(timeStr, timeX, timeY);

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Sand Box ---
#define SAND_WIDTH 120
#define SAND_HEIGHT 120
byte sand_grid[SAND_WIDTH][SAND_HEIGHT] = {0};

static void SandBoxWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        
        if(util_random(100) < 20) sand_grid[util_random(SAND_WIDTH)][0] = 1;
        menuSprite.fillSprite(TFT_BLACK);

        for(int y=SAND_HEIGHT-2; y>=0; y--) {
            for(int x=0; x<SAND_WIDTH; x++) {
                if(sand_grid[x][y] == 1) {
                    if(sand_grid[x][y+1] == 0) { sand_grid[x][y] = 0; sand_grid[x][y+1] = 1; }
                    else if (x > 0 && sand_grid[x-1][y+1] == 0) { sand_grid[x][y] = 0; sand_grid[x-1][y+1] = 1; }
                    else if (x < SAND_WIDTH-1 && sand_grid[x+1][y+1] == 0) { sand_grid[x][y] = 0; sand_grid[x+1][y+1] = 1; }
                }
            }
        }
        for(int y=0; y<SAND_HEIGHT; y++) {
            for(int x=0; x<SAND_WIDTH; x++) {
                if(sand_grid[x][y] == 1) menuSprite.drawPixel(x*2, y*2, TFT_YELLOW);
            }
        }
        
        drawCommonElements();

        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = tft.height()/2 + 20;
        menuSprite.drawString(timeStr, timeX, timeY);

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Progress Bar Watchface ---
#define PB_DATA_X 3
#define PB_BAR_WIDTH 180
#define PB_BAR_HEIGHT 10
#define PB_PERCENTAGE_TEXT_X (PB_DATA_X + PB_BAR_WIDTH + 10)

static void ProgressBarWatchface() {
    lastSyncMillis = millis() - syncInterval - 1;
    while(1) {
        handlePeriodicSync();
        handleHourlyChime();
        if (readButton()) {
            if (g_hourlyMusicTaskHandle != NULL) {
                vTaskDelete(g_hourlyMusicTaskHandle);
                g_hourlyMusicTaskHandle = NULL;
                noTone(BUZZER_PIN);
                stopBuzzerTask = true;
            }
            tone(BUZZER_PIN, 1500, 50);
            return;
        }

        getLocalTime(&timeinfo);
        menuSprite.fillSprite(TFT_BLACK);
        drawCommonElements(); 

        // --- Add Time Display ---
        menuSprite.setTextDatum(MC_DATUM);
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        menuSprite.setTextSize(4);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        
        int timeWidth = menuSprite.textWidth(timeStr);
        int timeX = tft.width()/2;
        int timeY = 115;
        menuSprite.drawString(timeStr, timeX, timeY); // Y-pos below the new date

        // Draw 0.1s digit
        int tenth = (millis() % 1000) / 100;
        menuSprite.setTextSize(1);
        menuSprite.drawString(String(tenth), timeX + timeWidth/2 + 10, timeY + 10);

        // --- Progress Bars ---
        menuSprite.setTextDatum(TL_DATUM);
        char buf[32];
        menuSprite.setTextSize(1);
        menuSprite.setTextColor(TFT_CYAN, TFT_BLACK);

        const int bar_y_start = 150; // Move bars down to make space for time
        const int bar_y_spacing = 30;

        // Day progress bar
        long seconds_in_day = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
        float day_progress = (float)seconds_in_day / (24.0 * 3600.0 - 1.0);
        menuSprite.fillRect(PB_DATA_X, bar_y_start, PB_BAR_WIDTH, PB_BAR_HEIGHT, TFT_DARKGREY);
        menuSprite.fillRect(PB_DATA_X, bar_y_start, (int)(PB_BAR_WIDTH * day_progress), PB_BAR_HEIGHT, TFT_RED);
        sprintf(buf, "Day: %.0f%%", day_progress * 100);
        menuSprite.drawString(buf, PB_PERCENTAGE_TEXT_X, bar_y_start);

        // Hour progress bar
        float hour_progress = (float)(timeinfo.tm_min * 60 + timeinfo.tm_sec) / 3599.0;
        menuSprite.fillRect(PB_DATA_X, bar_y_start + bar_y_spacing, PB_BAR_WIDTH, PB_BAR_HEIGHT, TFT_DARKGREY);
        menuSprite.fillRect(PB_DATA_X, bar_y_start + bar_y_spacing, (int)(PB_BAR_WIDTH * hour_progress), PB_BAR_HEIGHT, TFT_BLUE);
        sprintf(buf, "Hour: %.0f%%", hour_progress * 100);
        menuSprite.drawString(buf, PB_PERCENTAGE_TEXT_X, bar_y_start + bar_y_spacing);

        // Minute progress bar
        float minute_progress = (float)timeinfo.tm_sec / 59.0;
        menuSprite.fillRect(PB_DATA_X, bar_y_start + bar_y_spacing * 2, PB_BAR_WIDTH, PB_BAR_HEIGHT, TFT_DARKGREY);
        menuSprite.fillRect(PB_DATA_X, bar_y_start + bar_y_spacing * 2, (int)(PB_BAR_WIDTH * minute_progress), PB_BAR_HEIGHT, TFT_GREEN);
        sprintf(buf, "Min: %.0f%%", minute_progress * 100);
        menuSprite.drawString(buf, PB_PERCENTAGE_TEXT_X, bar_y_start + bar_y_spacing * 2);

        menuSprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}