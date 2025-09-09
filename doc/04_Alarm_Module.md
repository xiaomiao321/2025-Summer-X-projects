# 「持久化记忆」可配置闹钟系统的构建

## 一、模块概述

闹钟是时钟产品最古老也最核心的功能之一。本模块旨在实现一个功能强大、配置灵活且数据可持久化的闹C钟系统。它不仅仅是在特定时间响铃，更是一个支持多组闹钟、自定义重复周期、并且能在设备断电后依然保存用户设置的完整解决方案。

用户可以通过一个直观的列表界面，添加、删除、启用或禁用最多10组闹钟。对于每一组闹钟，用户可以精确地设置其响铃的小时和分钟，并能以“勾选”的方式自由组合每周需要重复的天数（如只在工作日、或只在周末响铃）。所有这些设置都会被保存在非易失性存储中，确保万无一失。

## 二、实现思路与技术选型

### 数据持久化：EEPROM的应用

闹钟设置属于用户配置，绝不能因为设备重启或断电而丢失。因此，我们需要一个非易失性存储方案。在ESP32上，最简单直接的选择就是**EEPROM**。我们使用`EEPROM.h`库来读写内部的模拟EEPROM。

-   **数据结构**：我们定义了一个`AlarmSetting`结构体，用于封装一个闹钟的所有属性（时、分、星期几的位掩码、是否启用等）。然后，我们创建了一个该结构体的数组`alarms[MAX_ALARMS]`作为内存中的数据副本。
-   **存取逻辑**：在`saveAlarms()`函数中，我们使用`EEPROM.put()`将整个`alarms`数组一次性写入EEPROM。在`loadAlarms()`中，则用`EEPROM.get()`读出。为了防止首次上电或EEPROM数据损坏时读到无效数据，我们还在EEPROM的起始位置写入了一个“魔数”（Magic Number, `0xAD`）。每次加载时，先检查这个魔数是否存在，如果不存在，就用默认值初始化整个闹钟列表，保证了系统的健壮性。(EEPROM是嵌入式开发中保存少量配置数据的常用方法，可以阅读 [Arduino官方EEPROM库文档](https://www.arduino.cc/en/Reference/EEPROM) 了解更多)

### 核心逻辑：后台任务与时间比对

闹钟的触发检查必须在后台持续、准时地进行，不能被任何前台UI操作（如用户正在玩游戏或设置倒计时）所阻塞。为此，我们创建了一个专用的FreeRTOS**后台任务**`Alarm_Check_Task`。

这个任务的逻辑非常纯粹：每秒钟醒来一次，获取当前的标准时间`timeinfo`，然后遍历内存中的`alarms`数组。对于每一个“已启用”的闹钟，它会进行一系列比对：
1.  **星期匹配**：通过位运算`alarms[i].days_of_week & (1 << current_day)`来判断今天是否是该闹钟设定的重复日。
2.  **时间匹配**：直接比对`alarms[i].hour == timeinfo.tm_hour`和`alarms[i].minute == timeinfo.tm_min`。
3.  **今日防重触发**：为了防止在一分钟内（如第0秒和第1秒）重复触发同一个闹钟，我们增加了一个`triggered_today`标志位。一旦闹钟触发，此标志即被设为`true`，直到第二天凌晨才会重置。这确保了每个闹钟每天最多只响一次。

当所有条件都满足时，就调用`triggerAlarm()`函数，开始响铃。

### 交互设计：列表视图与双击编辑

为了在一个屏幕内管理多达10个闹钟，我们设计了一个可滚动的列表视图。用户通过旋转编码器上下移动光标，短按单击可以快速“启用/禁用”当前选中的闹钟，这个操作非常高频，所以设计得最为便捷。

对于“编辑”这个相对低频但更复杂的操作，我们引入了**双击**的交互方式。用户在选中一个闹钟后，快速按两下按钮，即可进入专门的编辑界面。这种区分单击和双击的交互模式，可以在有限的按键下，为不同的操作赋予符合直觉的优先级，是嵌入式UI设计中的常用技巧。

## 三、代码深度解读

### 闹钟检查核心逻辑 (`Alarm.cpp`)

```cpp
void Alarm_Loop_Check() {
  extern struct tm timeinfo;
  if (timeinfo.tm_year < 100) return; // 确保时间已同步
  
  int current_day = timeinfo.tm_wday;
  // 每日重置触发标志
  if (last_checked_day != current_day) {
    for (int i = 0; i < alarm_count; ++i) { alarms[i].triggered_today = false; }
    last_checked_day = current_day;
    saveAlarms();
  }
  
  for (int i = 0; i < alarm_count; ++i) {
    if (g_alarm_is_ringing) break; // 如果正在响铃，则不检查新闹钟
    if (alarms[i].enabled && !alarms[i].triggered_today) {
      bool day_match = alarms[i].days_of_week & (1 << current_day);
      if (day_match && alarms[i].hour == timeinfo.tm_hour && alarms[i].minute == timeinfo.tm_min) {
        triggerAlarm(i);
      }
    }
  }
}
```

这是在后台任务中被反复调用的核心检查函数。它的逻辑严谨，考虑了多种边界情况：
-   `if (timeinfo.tm_year < 100) return;`：这是一个保护性代码，防止在设备刚启动、NTP时间尚未同步成功时（此时年份通常是一个无效的小值）进行错误的闹钟比对。
-   `if (last_checked_day != current_day)`：通过这个判断，程序能准确地捕捉到新的一天到来的瞬间，并在此刻重置所有闹钟的`triggered_today`标志，为新一天的闹钟触发做好准备。
-   `bool day_match = alarms[i].days_of_week & (1 << current_day);`：这是一个高效的位运算技巧。`days_of_week`是一个8位的`uint8_t`，每一位代表一周中的一天（如第0位代表周日，第1位代表周一）。`1 << current_day`会生成一个只有当天对应的位是1的掩码。通过按位与（`&`）运算，如果结果不为0，就说明闹钟设置中包含了今天。

### 闹钟编辑界面 (`Alarm.cpp`)

```cpp
static void editAlarm(int index) {
    // ... 初始化temp_alarm ...
    EditMode edit_mode = EDIT_HOUR;
    int day_cursor = 0;

    while(true) {
        // ... 编码器输入处理 ...
        switch(edit_mode) {
            case EDIT_HOUR: temp_alarm.hour = (temp_alarm.hour + encoder_value + 24) % 24; break;
            case EDIT_MINUTE: temp_alarm.minute = (temp_alarm.minute + encoder_value + 60) % 60; break;
            case EDIT_DAYS: day_cursor = (day_cursor + encoder_value + 7) % 7; break;
            // ...
        }

        // ... 按键输入处理 ...
        if (readButton()) {
            if (edit_mode == EDIT_DAYS) {
                temp_alarm.days_of_week ^= (1 << day_cursor); // 切换选中日的使能状态
            } else if (edit_mode == EDIT_SAVE) {
                // ... 保存逻辑 ...
            } else if (edit_mode == EDIT_DELETE) {
                // ... 删除逻辑 ...
            }
            edit_mode = (EditMode)((edit_mode + 1) % 5); // 切换编辑模式
            drawEditScreen(temp_alarm, edit_mode, day_cursor);
        }
    }
}
```

编辑界面同样是一个状态机，通过`EditMode`枚举（`EDIT_HOUR`, `EDIT_MINUTE`, `EDIT_DAYS`等）来管理当前用户的编辑焦点。用户通过按键在不同的编辑模式间切换，通过旋转编码器来修改对应模式下的值。在`EDIT_DAYS`模式下，`temp_alarm.days_of_week ^= (1 << day_cursor)`这行代码再次展示了位运算的强大之处。通过按位异或（`^`），它可以快速地翻转（toggle）星期掩码中对应`day_cursor`的那一位，实现“选中”或“取消选中”某天的效果，代码非常简洁高效。

## 四、效果展示与体验优化

最终的闹钟模块功能稳定，交互清晰。列表页和编辑页的视觉反馈明确，用户可以轻松完成所有配置。后台任务的引入，保证了无论用户正在使用何种前台功能，都不会错过任何一个闹钟提醒。当闹钟触发时，设备会进入一个专有的响铃界面，并持续播放音乐，直到用户按下按钮停止，这是一个符合用户使用习惯的完整闭环。

**[此处应有实际效果图，展示闹钟列表、编辑界面和响铃界面]**

## 五、总结与展望

闹钟模块通过**EEPROM持久化**、**后台任务轮询**和**精巧的UI交互设计**，构建了一个健壮而实用的核心功能。它不仅是一个简单的定时器，更是一个完整的、可管理的数据应用。位运算在本模块中对星期进行编码和操作的技巧，是嵌入式开发中处理多状态组合的典型范例。

未来的可扩展方向可以是：
-   **自定义铃声**：允许用户为不同的闹钟设置不同的铃声。
-   **贪睡功能（Snooze）**：在闹钟响起时，提供一个“稍后提醒”的选项。
-   **与日历同步**：如果能接入更复杂的日历服务，可以实现基于事件的智能提醒，而不仅仅是基于时间的闹钟。