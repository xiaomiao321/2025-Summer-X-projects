#ifndef ALARM_H
#define ALARM_H

// Flag to indicate that an alarm is currently ringing
extern volatile bool g_alarm_is_ringing;

// Initializes the alarm module and starts the background checking task
void Alarm_Init();

// The main UI for the alarm settings
void AlarmMenu();

// The UI to show when an alarm is ringing
void Alarm_ShowRingingScreen();

// Stops the currently playing alarm music
void Alarm_StopMusic();

#endif // ALARM_H
