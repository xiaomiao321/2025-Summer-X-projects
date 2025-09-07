#ifndef SYSTEM_H
#define SYSTEM_H


extern int tft_log_y;
extern int current_log_lines;
const int LOG_MARGIN = 5;
const int LOG_LINE_HEIGHT = 12;
const int LOG_MAX_LINES = 15;


void bootSystem();
void tftLog(String text, uint16_t color);
void tftLogInfo(const String& text);
void tftLogWarning(const String& text);
void tftLogError(const String& text);
void tftLogSuccess(const String& text);
void tftLogDebug(const String& text);
void tftLogWithIcon(const String& text, uint16_t color, const char* icon);
void tftLogProgress(const String& text, int progress, int total);
void tftClearLog();
#endif // !SYSTEM_H