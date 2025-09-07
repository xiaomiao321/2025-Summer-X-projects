#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>

extern volatile bool g_alarm_is_ringing; // Make this global flag accessible

// Moved from .cpp to be public
struct AlarmSetting {
  uint8_t hour;
  uint8_t minute;
  uint8_t days_of_week;
  bool enabled;
  bool triggered_today;
};

void Alarm_Init();
void AlarmMenu();
void Alarm_Loop_Check();
void Alarm_ShowRingingScreen();
void Alarm_StopMusic();

// New functions to get alarm data
int getAlarmCount();
bool getAlarmInfo(int index, AlarmSetting& settings);


#endif // ALARM_H
