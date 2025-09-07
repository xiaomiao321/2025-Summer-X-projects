#include "Pomodoro.h"
#include "Alarm.h"
#include "Menu.h"
#include "RotaryEncoder.h"
#include "Buzzer.h"
#include "animation.h" // For smoothArc
#include "Alarm.h"
#include "weather.h"
// --- Configuration ---
const unsigned long WORK_DURATION_SECS = 25 * 60;
const unsigned long SHORT_BREAK_DURATION_SECS = 5 * 60;
const unsigned long LONG_BREAK_DURATION_SECS = 15 * 60;
const int SESSIONS_BEFORE_LONG_BREAK = 4;

// --- State Machine ---
enum PomodoroState { 
    STATE_IDLE, 
    STATE_WORK, 
    STATE_SHORT_BREAK, 
    STATE_LONG_BREAK, 
    STATE_PAUSED 
};

// --- Module-static State Variables ---
static PomodoroState currentState = STATE_IDLE;
static PomodoroState stateBeforePause = STATE_IDLE;
static unsigned long session_end_millis = 0;
static unsigned long remaining_on_pause = 0;
static int sessions_completed = 0;
static unsigned long last_pomodoro_beep_time = 0; // For 5-second warning

// =====================================================================================
//                                     DRAWING LOGIC
// =====================================================================================

static void drawPomodoroUI(unsigned long remaining_secs, unsigned long total_secs) {
    menuSprite.fillScreen(TFT_BLACK);

    // Display current time at the top
    if (!getLocalTime(&timeinfo)) {
        // Handle error or display placeholder
    } else {
        char time_str[30]; // Increased buffer size
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %a", &timeinfo); // New format
        menuSprite.setTextFont(2); // Smaller font for time
        menuSprite.setTextSize(1);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        menuSprite.setTextDatum(MC_DATUM); // Center align
        menuSprite.drawString(time_str, menuSprite.width() / 2, 10); // Position at top center
        menuSprite.setTextDatum(TL_DATUM); // Reset datum
    }

    menuSprite.setTextDatum(MC_DATUM);

    // --- Draw Title / Current State ---
    menuSprite.setTextFont(1);
    menuSprite.setTextColor(TFT_WHITE);
    const char* state_text = "";
    uint16_t arc_color = TFT_DARKGREY;
    switch(currentState) {
        case STATE_IDLE: state_text = "Ready"; arc_color = TFT_SKYBLUE; break;
        case STATE_WORK: state_text = "Work"; arc_color = TFT_ORANGE; break;
        case STATE_SHORT_BREAK: state_text = "Short Break"; arc_color = TFT_GREEN; break;
        case STATE_LONG_BREAK: state_text = "Long Break"; arc_color = TFT_CYAN; break;
        case STATE_PAUSED: state_text = "Paused"; arc_color = TFT_YELLOW; break;
    }
    menuSprite.setTextSize(2);
    menuSprite.drawString(state_text, 120,180 );
    menuSprite.setTextSize(1);

    // --- Draw Circular Progress Bar (Radius 55) ---
    float progress = (total_secs > 0) ? (float)remaining_secs / total_secs : 0;
    int angle = 360 * progress;
    // Arc center (x,y) = (120, 115)
    // Radius = 55
    // Thickness = 10
    menuSprite.drawSmoothArc(120, 120, 100, 80, 0, 360, TFT_DARKGREY, TFT_BLACK, true); // Background arc
    menuSprite.drawSmoothArc(120, 120, 100-5, 80+5, 0, angle, arc_color, TFT_BLACK, true);    // Foreground arc

    // --- Draw Time (Font 7, Size 1 - consistent with Countdown.cpp) ---
    menuSprite.setTextFont(7);
    menuSprite.setTextSize(1);
    menuSprite.setTextColor(TFT_WHITE);
    char time_buf[6];
    sprintf(time_buf, "%02lu:%02lu", remaining_secs / 60, remaining_secs % 60);
    menuSprite.drawString(time_buf, 120, 115);

    // --- Draw Session Markers ---
    int marker_y = 230;
    int marker_total_width = SESSIONS_BEFORE_LONG_BREAK * 20;
    int marker_start_x = 120 - (marker_total_width / 2);
    for (int i = 0; i < SESSIONS_BEFORE_LONG_BREAK; i++) {
        if (i < sessions_completed) {
            menuSprite.fillCircle(marker_start_x + (i * 20), marker_y, 6, TFT_GREEN);
        } else {
            menuSprite.drawCircle(marker_start_x + (i * 20), marker_y, 6, TFT_DARKGREY);
        }
    }

    menuSprite.pushSprite(0, 0);
}

// =====================================================================================
//                                     CORE LOGIC
// =====================================================================================

static void startNewSession(PomodoroState nextState) {
    currentState = nextState;
    unsigned long duration_secs = 0;

    switch(currentState) {
        case STATE_WORK: duration_secs = WORK_DURATION_SECS; break;
        case STATE_SHORT_BREAK: duration_secs = SHORT_BREAK_DURATION_SECS; break;
        case STATE_LONG_BREAK: 
            duration_secs = LONG_BREAK_DURATION_SECS; 
            sessions_completed = 0;
            break;
        default: break;
    }

    session_end_millis = millis() + (duration_secs * 1000);
    drawPomodoroUI(duration_secs, duration_secs);
    last_pomodoro_beep_time = 0; // Reset beep timer
}

void PomodoroMenu() {
    currentState = STATE_IDLE;
    sessions_completed = 0;
    drawPomodoroUI(WORK_DURATION_SECS, WORK_DURATION_SECS); // Initial draw for IDLE state
    unsigned long last_draw_time = 0; // For controlling redraw frequency
    unsigned long last_realtime_clock_update = millis(); // For real-time clock update

    while(true) {
        if (g_alarm_is_ringing) { return; } // ADDED LINE
        if (readButtonLongPress()) {
            tone(BUZZER_PIN, 1500, 100);
            menuSprite.setTextFont(1); // Reset font on exit
            menuSprite.setTextSize(1);
            return; // Reset and Exit
        }

        if (readButton()) {
            tone(BUZZER_PIN, 2000, 50);
            if (currentState == STATE_IDLE) {
                startNewSession(STATE_WORK);
            } else if (currentState == STATE_PAUSED) {
                currentState = stateBeforePause;
                session_end_millis = millis() + remaining_on_pause;
            } else { // Any running state
                remaining_on_pause = session_end_millis - millis();
                stateBeforePause = currentState;
                currentState = STATE_PAUSED;
                // Recalculate total_secs for paused state display
                unsigned long total_secs_on_pause = 0;
                 switch(stateBeforePause) {
                    case STATE_WORK: total_secs_on_pause = WORK_DURATION_SECS; break;
                    case STATE_SHORT_BREAK: total_secs_on_pause = SHORT_BREAK_DURATION_SECS; break;
                    case STATE_LONG_BREAK: total_secs_on_pause = LONG_BREAK_DURATION_SECS; break;
                    default: break;
                }
                drawPomodoroUI(remaining_on_pause / 1000, total_secs_on_pause);
            }
        }

        // --- Timer tick logic ---
        if (currentState != STATE_IDLE && currentState != STATE_PAUSED) {
            unsigned long time_now = millis();
            unsigned long remaining_ms = (time_now >= session_end_millis) ? 0 : session_end_millis - time_now;
            
            // Redraw every second to keep the timer ticking
            if (time_now - last_draw_time >= 990) { // Use >= 990 to ensure redraws happen once per second
                unsigned long remaining_secs = remaining_ms / 1000;
                unsigned long total_secs = 0;
                switch(currentState) {
                    case STATE_WORK: total_secs = WORK_DURATION_SECS; break;
                    case STATE_SHORT_BREAK: total_secs = SHORT_BREAK_DURATION_SECS; break;
                    case STATE_LONG_BREAK: total_secs = LONG_BREAK_DURATION_SECS; break;
                    default: break;
                }
                drawPomodoroUI(remaining_secs, total_secs);
                last_draw_time = time_now;
            }

            // 5-second warning
            if (remaining_ms > 0 && remaining_ms <= 5000 && (time_now - last_pomodoro_beep_time >= 1000 || last_pomodoro_beep_time == 0)) {
                tone(BUZZER_PIN, 1000, 100);
                last_pomodoro_beep_time = time_now;
            }

            if (remaining_ms == 0) {
                tone(BUZZER_PIN, 3000, 3000); // Final alarm sound
                if (currentState == STATE_WORK) {
                    sessions_completed++;
                    if (sessions_completed >= SESSIONS_BEFORE_LONG_BREAK) {
                        startNewSession(STATE_LONG_BREAK);
                    } else {
                        startNewSession(STATE_SHORT_BREAK);
                    }
                } else { // A break finished
                    startNewSession(STATE_WORK);
                }
            }
        } else { // If not running (IDLE or PAUSED), update real-time clock
            unsigned long current_millis = millis();
            if (current_millis - last_realtime_clock_update >= 1000) { // Update every second
                unsigned long current_remaining_secs = 0;
                unsigned long current_total_secs = 0;

                if (currentState == STATE_IDLE) {
                    current_remaining_secs = WORK_DURATION_SECS;
                    current_total_secs = WORK_DURATION_SECS;
                } else if (currentState == STATE_PAUSED) {
                    current_remaining_secs = remaining_on_pause / 1000;
                    switch(stateBeforePause) {
                        case STATE_WORK: current_total_secs = WORK_DURATION_SECS; break;
                        case STATE_SHORT_BREAK: current_total_secs = SHORT_BREAK_DURATION_SECS; break;
                        case STATE_LONG_BREAK: current_total_secs = LONG_BREAK_DURATION_SECS; break;
                        default: current_total_secs = WORK_DURATION_SECS; break; // Fallback
                    }
                }
                drawPomodoroUI(current_remaining_secs, current_total_secs);
                last_realtime_clock_update = current_millis;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
