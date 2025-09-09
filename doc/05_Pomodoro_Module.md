# 「高效时间法」番茄工作法模块的实现

## 一、模块概述

番茄工作法是一种著名的时间管理方法，它将工作分解为多个25分钟的专注时段，中间穿插短暂的休息，以提高注意力和效率。本模块在我们的多功能时钟上，完整地实现了这一流程，旨在帮助用户更好地管理他们的工作与休息节奏。

该模块严格遵循番茄工作法的核心循环：
1.  开始一个25分钟的“工作”时段。
2.  工作结束后，进入一个5分钟的“短休息”。
3.  每完成4个“工作”时段，会进入一个15分钟的“长休息”。
4.  在任何时段，用户都可以暂停和恢复计时。

整个过程通过一个美观的圆形进度条和清晰的状态文本进行可视化，让用户对当前所处阶段和剩余时间一目了然。

## 二、实现思路与技术选型

### 核心逻辑：基于枚举的状态机

番茄钟的本质是一个在不同状态间自动流转的计时器。为了清晰地管理这些状态（如空闲、工作、短休息、长休息、暂停），我们使用了一个**枚举类型`PomodoroState`**。这是实现状态机最直观、最易读的方法。

-   **状态变量**：我们定义了`currentState`来保存当前的状态，`stateBeforePause`用于记录暂停前所处的状态，以便恢复。`sessions_completed`则用于计数，以决定何时应该进入长休息。
-   **状态转移**：状态的转移由计时结束事件触发。例如，当一个`STATE_WORK`的计时结束时，程序会检查`sessions_completed`的计数值，如果计数值达到4，则调用`startNewSession(STATE_LONG_BREAK)`切换到长休息状态；否则，调用`startNewSession(STATE_SHORT_BREAK)`进入短休息。这种清晰的逻辑分离使得整个工作流的管理变得非常简单。

### UI设计：圆形进度条与信息可视化

为了提供比线性进度条更具沉浸感的体验，我们选择了一个**圆环形的进度条**来作为核心视觉元素。这个圆环不仅美观，也暗合了时钟的“圆”形意象。

-   **绘制实现**：我们没有使用`TFT_eSPI`库中标准的`drawArc`，因为它绘制的弧线有锯齿。我们实现了一个自定义的`drawSmoothArc`函数（在`animation.cpp`中），它通过在多个稍有差异的半径上绘制同心圆弧，并利用人眼的视觉混合效应，来模拟出“抗锯齿”的平滑边缘效果。这是一种在不引入复杂图形库的情况下，提升绘图质量的实用技巧。
-   **信息分层**：我们将信息清晰地分成了几个层次：屏幕顶部的全局时间、圆环中心的倒计时数字、下方的当前状态文本，以及最底部的已完成工作时段计数器。这种布局主次分明，用户可以快速获取到他们最关心的信息。

## 三、代码深度解读

### 状态切换的核心函数 (`Pomodoro.cpp`)

```cpp
static void startNewSession(PomodoroState nextState) {
    currentState = nextState;
    unsigned long duration_secs = 0;

    switch(currentState) {
        case STATE_WORK: duration_secs = WORK_DURATION_SECS; break;
        case STATE_SHORT_BREAK: duration_secs = SHORT_BREAK_DURATION_SECS; break;
        case STATE_LONG_BREAK: 
            duration_secs = LONG_BREAK_DURATION_SECS; 
            sessions_completed = 0; // 开始长休息时，重置计数器
            break;
        default: break;
    }

    session_end_millis = millis() + (duration_secs * 1000);
    drawPomodoroUI(duration_secs, duration_secs);
    last_pomodoro_beep_time = 0; // 重置结束前提示音的计时器
}
```

`startNewSession`是整个模块的“引擎”，负责启动任意一个新的计时阶段。它的职责单一且明确：
1.  更新全局的`currentState`变量。
2.  根据新的状态，从预设的常量中查询出对应的持续时间。
3.  特别地，在进入`STATE_LONG_BREAK`时，它会重置`sessions_completed`计数器，为一个新的大循环做准备。
4.  计算出本阶段的目标结束`millis()`时间戳。
5.  调用UI函数，刷新屏幕显示，进入新的阶段。

将所有开启新阶段的公共逻辑都收敛到这一个函数中，极大地简化了代码，避免了在多个地方出现重复的逻辑。

### 主循环中的计时与状态转移 (`Pomodoro.cpp`)

```cpp
// --- Timer tick logic ---
if (currentState != STATE_IDLE && currentState != STATE_PAUSED) {
    unsigned long time_now = millis();
    unsigned long remaining_ms = (time_now >= session_end_millis) ? 0 : session_end_millis - time_now;
    
    // ... 定时重绘UI ...

    if (remaining_ms == 0) { // 计时结束
        tone(BUZZER_PIN, 3000, 3000); // 结束提示音
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
}
```

这段代码位于主`while`循环中，是状态机运转的核心驱动力。它只在计时器“正在运行”（非空闲也非暂停）时执行。`remaining_ms == 0`这个条件判断准确地捕捉了计时结束的瞬间。此时，程序会根据当前的`currentState`来决定下一个状态应该是什么：
-   如果一个`STATE_WORK`结束了，就增加`sessions_completed`计数，并根据计数值决定接下来是进入长休息还是短休息。
-   如果是一个休息状态（`STATE_SHORT_BREAK`或`STATE_LONG_BREAK`）结束了，那么唯一可能的下一个状态就是`STATE_WORK`。

通过这种方式，程序自动地在“工作-休息”的循环中运转，完美地实现了番茄工作法的流程。

## 四、效果展示与体验优化

最终实现的番茄钟模块，界面美观，逻辑自洽。平滑的圆形进度条提供了优秀的视觉反馈。用户通过简单的单击操作即可控制整个流程的启停。状态文本和会话计数器让用户对整个工作周期有清晰的掌控感。与倒计时模块类似，结束前5秒的预警音和结束时的长音，也提供了及时的听觉反馈。

**[此处应有实际效果图，展示工作、短休息、长休息和暂停时的界面]**

## 五、总结与展望

番茄钟模块是状态机设计模式在嵌入式UI应用中的一个绝佳范例。通过清晰的枚举状态和精确的`millis()`计时，我们构建了一个行为可预测、逻辑稳健的时间管理工具。自定义的平滑圆弧绘制函数也展示了在资源有限的平台上，通过巧妙的算法提升视觉效果的可能性。

未来的可扩展方向可以是：
-   **自定义时长**：允许用户在设置中自定义工作、短休息和长休息的时长，以适应不同的工作习惯。
-   **任务列表**：在开始一个工作时段前，允许用户简单输入当前要专注的任务名称，并在工作时段中显示。
-   **数据统计**：记录每天完成的番茄钟数量，并提供一个简单的统计图表，以激励用户。