# 「模拟之眼」ADC与光敏传感器的数据采集

## 一、模块概述

数字世界与模拟世界的桥梁是ADC（Analog-to-Digital Converter，模数转换器）。本模块专门用于演示ESP32强大的ADC功能，通过读取一个连接在模拟引脚上的光敏电阻分压电路的电压，来实时测量环境光照强度（LUX），并将其用多种方式可视化地呈现在屏幕上。

此模块的功能点包括：
1.  初始化并配置ESP32的ADC1通道。
2.  利用eFuse中的校准值，对ADC读数进行精确校准。
3.  在后台任务中持续读取ADC原始值，并将其转换为电压。
4.  根据光敏电阻的特性曲线公式，将电压值进一步换算为光照强度（LUX）。
5.  通过一个拟物化的电压表、一个直观的进度条和精确的数字读数，全方位地展示采集到的数据。

## 二、实现思路与技术选型

### 硬件交互：ESP-IDF ADC校准

消费级的ADC外设，其读数往往会存在一定的非线性误差。为了获取更精确的电压值，ESP32在出厂时，在内部的eFuse（一种一次性可编程存储器）中烧录了针对当前芯片ADC特性的校准值。我们可以利用这些校准值来修正我们的读数。

-   **ADC校准**：我们没有直接使用Arduino的`analogRead()`，而是调用了更底层的ESP-IDF驱动函数。`adc_calibration_init()`函数会检查eFuse中是否存在校准值，如果存在，则调用`esp_adc_cal_characterize()`来初始化一个包含校准曲线参数的`adc1_chars`结构体。之后，我们就可以用`esp_adc_cal_raw_to_voltage()`函数，将ADC的原始读数（0-4095）直接转换为非常精确的毫伏电压值。这是一个在需要精密模拟测量的项目中，强烈推荐使用的官方功能。(要深入了解ESP32的ADC特性，可以阅读 [乐鑫官方的ADC文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html))
-   **多重采样与平均**：为了减少单次读数中可能存在的噪声和毛刺，我们采用了多重采样的策略。在每次测量时，程序会连续读取50次ADC的原始值，然后取其平均值。这种简单的滑动平均滤波，能有效地提高读数的稳定性。

### 数据处理：从电压到勒克斯（LUX）

我们从ADC直接得到的是电压值，但我们想显示的是光照强度单位——勒克斯（LUX）。这需要一个转换公式。对于一个典型的光敏电阻（LDR）与一个固定电阻串联的分压电路，其电压与光照强度的关系通常是非线性的，可以通过一个对数或幂函数来近似。

-   **电阻计算**：首先，根据分压公式 `V_out = V_in * R_ldr / (R_fixed + R_ldr)`，我们可以反推出光敏电阻当前的阻值 `r_photo = (voltage_v * R_FIXED) / (3.3f - voltage_v)`。
-   **LUX转换**：光敏电阻的阻值与光照强度的关系通常是 `R = R10 * (LUX / 10) ^ -GAMMA`。其中`R10`是光敏电阻在10LUX光照下的标准阻值，`GAMMA`是其特性系数。通过反解这个公式，我们就能得到最终的LUX值：`lux = pow((r_photo / R10), (1.0f / -GAMMA)) * 10.0f`。这个公式的运用，是实现从电学量到物理量转换的关键。

### 数据可视化：拟物仪表盘

为了让电压的显示更生动，我们再次使用了`TFT_eWidget`库中的`MeterWidget`。这个小部件可以模拟一个经典的指针式电压表。我们只需在初始化时设置好量程和刻度，然后在主循环中调用`volts.updateNeedle(voltage_v, 0)`，就能驱动表盘上的指针偏转到对应的电压值，非常直观和复古。

## 三、代码深度解读

### ADC初始化与校准 (`ADC.cpp`)

```cpp
static esp_adc_cal_characteristics_t adc1_chars;
bool cali_enable = false;

static bool adc_calibration_init() {
    esp_err_t ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret != ESP_OK) return false; // 如果eFuse中没有校准值，则直接返回

    // 获取校准特性参数
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 0, &adc1_chars);
    return true;
}

void setupADC() {
    adc1_config_width(ADC_WIDTH_BIT_12); // 设置ADC位宽为12位
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_12); // 设置衰减，决定了可测量电压范围
    cali_enable = adc_calibration_init(); // 尝试进行校准
}
```

这段代码展示了ADC的正确初始化流程。`setupADC`函数配置了ADC的工作参数，最关键的一步是调用`adc_calibration_init()`。这个函数首先检查eFuse中是否存在可用的校准信息。如果存在，它就会调用`esp_adc_cal_characterize`来生成校准数据，并设置全局标志位`cali_enable`为`true`。在后续的测量中，程序会根据这个标志位来决定是使用经过校准的转换函数，还是使用标准的线性估算，这保证了代码的兼容性和准确性。

### 后台数据采集与处理任务 (`ADC.cpp`)

```cpp
void ADC_Task(void *pvParameters) {
    // ... 初始化电压表 ...
    while (!stopADCTask) {
        // 1. 多重采样求平均值
        uint32_t sum = 0;
        for (int i = 0; i < 50; i++) { sum += adc1_get_raw(ADC_CHANNEL); delay(1); }
        sum /= 50;

        // 2. ADC原始值转电压（带校准）
        float voltage_v = 0;
        if (cali_enable) {
            uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(sum, &adc1_chars);
            voltage_v = voltage_mv / 1000.0f;
        } else { // 如果校准失败，则使用理论公式
            voltage_v = (sum * 3.3) / 4095.0;
        }
        volts.updateNeedle(voltage_v, 0); // 更新电压表指针

        // 3. 电压转LUX
        float r_photo = (voltage_v * R_FIXED) / (3.3f - voltage_v);
        float lux = pow((r_photo / R10), (1.0f / -GAMMA)) * 10.0f;

        // 4. 使用Sprite无闪烁地更新屏幕显示
        menuSprite.fillSprite(TFT_BLACK);
        // ... 绘制LUX值、电压值、ADC原始值和进度条 ...
        menuSprite.pushSprite(0, 130);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}
```

`ADC_Task`是一个独立的后台任务，它完整地执行了“采集-处理-显示”的流程。代码的执行步骤清晰地反映了我们的设计思路：从多重采样、到电压转换、再到光照度计算。最后，它将所有计算结果绘制在一个离屏的`menuSprite`上，再一次性推送到屏幕，从而避免了因分步绘制多个数据项而导致的屏幕闪烁问题。这个任务是整个模块的核心，它独立完成了所有工作。

## 四、效果展示与体验优化

最终的ADC模块提供了一个专业且信息丰富的模拟传感器读数界面。用户不仅能看到最终的光照度（LUX）值，还能看到中间的电压值和最底层的ADC原始读数，这对于学习和调试非常有帮助。拟物化的电压表和直观的LUX进度条，让数据的变化趋势一目了然。用手遮挡光敏电阻，可以看到指针和进度条实时、灵敏地做出反应。

**[此处应有实际效果图或GIF，展示ADC模块的完整界面，并演示用手遮挡光敏电阻时，各项读数和图表实时变化的效果]**

## 五、总结与展望

ADC模块是连接物理世界与数字世界的桥梁。通过这个模块的实现，我们掌握了ESP32上ADC的精确使用方法，包括**官方校准、多重采样滤波**，以及如何根据传感器特性将电学量转换为有意义的物理量。它是一个功能虽小但技术细节丰富的模块，是开发各类模拟传感器应用的基础。

未来的可扩展方向可以是：
-   **自动亮度调节**：将此模块的功能化用到整个系统中。根据测量到的环境光照强度，自动调节TFT屏幕的背光亮度和LED灯带的亮度，实现智能化的节能和护眼功能。
-   **支持更多模拟传感器**：利用ADC，我们可以接入各种各样的模拟传感器，如声音传感器（麦克风）、土壤湿度传感器、气体传感器等，将我们的时钟变成一个更多功能的环境监测站。
-   **数据校准界面**：增加一个高级功能，允许用户在已知的光照条件下（如用标准照度计测量），手动输入当前的LUX值，程序可以反向计算并微调`GAMMA`等参数，实现对特定光敏电阻的精确校准。