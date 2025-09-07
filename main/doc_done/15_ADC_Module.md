# ADC模块：模拟信号读取与光照度监测

## 一、引言

ADC（Analog-to-Digital Converter）模块负责读取模拟传感器（例如光敏电阻）的信号，并将其转换为数字值。本模块旨在展示如何配置和使用ESP32的ADC功能，将模拟电压转换为实际物理量（如光照度Lux），并通过图形化界面直观地显示这些数据。

## 二、实现思路与技术选型

### 2.1 ADC配置与校准

- **ADC配置**: 使用ESP32的ADC1，配置其位宽（`ADC_WIDTH_BIT_12`，即12位分辨率）和衰减（`ADC_ATTEN_DB_12`，适用于0-3.6V的输入范围）。
- **ADC校准**: 为了提高ADC读取的准确性，模块利用 `esp_adc_cal` 库进行ADC校准。该库可以根据ESP32芯片内部的eFuse校准数据，将原始ADC值更精确地转换为电压值，减少测量误差。

### 2.2 模拟信号读取与转换

- **原始值读取**: 通过 `adc1_get_raw()` 函数周期性地读取指定ADC通道的原始数字值。
- **多样本平均**: 为了减少噪声和提高稳定性，模块会读取多个样本并计算其平均值。
- **电压转换**: 将原始ADC值通过校准数据（如果可用）或线性映射转换为电压值。

### 2.3 光照度（Lux）计算

模块将读取到的电压值进一步转换为光照度（Lux）。这通常涉及到光敏电阻的特性曲线，通过一个公式将电阻值（由电压和固定电阻计算得出）映射到光照度。公式中包含 `R_FIXED`（分压电阻）、`R10`（光敏电阻在10 Lux时的阻值）和 `GAMMA`（光敏电阻的伽马值）等参数。

### 2.4 用户界面与图形化展示

模块使用 `TFT_eSPI` 和 `TFT_eWidget` 库来构建丰富的图形化界面：
- **模拟仪表**: 使用 `MeterWidget` 绘制一个模拟仪表盘，直观地显示电压值。
- **数值显示**: 屏幕上显示计算出的光照度（Lux）、原始ADC值和电压值。
- **进度条**: 一个水平进度条直观地显示光照度级别。
- **双缓冲**: 所有绘制操作都在 `menuSprite` 上进行，最后通过 `menuSprite.pushSprite()` 推送到屏幕，避免闪烁。

### 2.5 FreeRTOS任务管理

ADC的读取和UI的更新在一个独立的FreeRTOS任务 `ADC_Task` 中运行。这确保了数据的实时性，并且不会阻塞主循环。任务会持续运行，直到接收到停止信号。

### 2.6 任务控制与退出

- **停止标志**: `stopADCTask` 是一个 `volatile bool` 类型的全局标志，用于通知 `ADC_Task` 停止运行。当用户退出ADC模块或闹钟响铃时，此标志会被设置为 `true`。
- **任务自删除**: `ADC_Task` 在检测到停止标志后，会自行删除任务，释放资源。

### 2.7 与其他模块的集成

- **闹钟冲突**: 通过 `g_alarm_is_ringing` 全局标志，确保在闹钟响铃时，ADC模块能够及时退出，避免功能冲突。

## 三、代码展示

### `ADC.cpp`

```c++
#include <Arduino.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include "Menu.h"
#include "MQTT.h"
#include "RotaryEncoder.h"
#include "Alarm.h"
#include <math.h>

MeterWidget volts = MeterWidget(&tft);

#define BAT_PIN         2
#define ADC_CHANNEL     ADC1_CHANNEL_2
#define ADC_ATTEN       ADC_ATTEN_DB_12
#define ADC_WIDTH       ADC_WIDTH_BIT_12

static esp_adc_cal_characteristics_t adc1_chars;
bool cali_enable = false;
bool stopADCTask = false;

static bool adc_calibration_init() {
    esp_err_t ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret == ESP_ERR_NOT_SUPPORTED) return false;
    if (ret == ESP_ERR_INVALID_VERSION) return false;
    if (ret == ESP_OK) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 0, &adc1_chars);
        return true;
    }
    return false;
}

void setupADC() {
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    cali_enable = adc_calibration_init();
}

void ADC_Task(void *pvParameters) {
    volts.analogMeter(0, 0, 3.3f, "V", "0", "0.8", "1.6", "2.4", "3.3");

    const float R_FIXED = 20000.0f;
    const float R10 = 8000.0f;
    const float GAMMA = 0.6f;

    while (!stopADCTask) {
        uint32_t sum = 0;
        const int samples = 50;
        for (int i = 0; i < samples; i++) {
            sum += adc1_get_raw(ADC_CHANNEL);
            delay(1);
        }
        sum /= samples;

        float voltage_v = 0;
        if (cali_enable) {
            uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(sum, &adc1_chars);
            voltage_v = voltage_mv / 1000.0f;
        } else {
            voltage_v = (sum * 3.3) / 4095.0;
        }
        volts.updateNeedle(voltage_v, 0);

        // --- Flicker-free display updates using sprite ---
        menuSprite.fillSprite(TFT_BLACK);
        menuSprite.setTextSize(2);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

        // Calculate and display Lux
        float r_photo = (voltage_v * R_FIXED) / (3.3f - voltage_v);
        float lux = pow((r_photo / R10), (1.0f / -GAMMA)) * 10.0f;

        char luxStr[10];
        dtostrf(lux, 4, 1, luxStr);
        menuSprite.setCursor(20, 10);
        menuSprite.print("LUX: "); menuSprite.print(luxStr);

        // Display Voltage and ADC Raw Value
        char voltStr[10];
        dtostrf(voltage_v, 4, 2, voltStr);
        menuSprite.setCursor(20, 35);
        menuSprite.print("VOL: "); menuSprite.print(voltStr);

        menuSprite.setCursor(20, 60);
        menuSprite.print("ADC: "); menuSprite.print(sum);

        // Draw Progress Bar
        float constrainedLux = constrain(lux, 0.0f, 1000.0f); //
        int barWidth = map((long)constrainedLux, 0L, 100L, 0L, 200L); 
        menuSprite.drawRect(20, 85, 202, 22, TFT_WHITE);
        menuSprite.fillRect(21, 86, 200, 20, TFT_BLACK);
        menuSprite.fillRect(21, 86, barWidth, 20, TFT_GREEN);

        menuSprite.pushSprite(0, 130);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

void ADCMenu() {
    stopADCTask = false;
    tft.fillScreen(TFT_BLACK);
    
    xTaskCreatePinnedToCore(ADC_Task, "ADC_Task", 4096, NULL, 1, NULL, 0);

    while (1) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            stopADCTask = true; // Signal the background task to stop
            vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
            break;
        }
        if (g_alarm_is_ringing) { // ADDED LINE
            stopADCTask = true; // Signal the background task to stop
            vTaskDelay(pdMS_TO_TICKS(200)); // Give task time to stop
            break; // Exit loop
        }
        if (readButton()) {
            stopADCTask = true;
            vTaskDelay(pdMS_TO_TICKS(200)); 
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}

    ```

## 四、代码解读

### 4.1 ADC配置与校准

```c++
#define BAT_PIN         2
#define ADC_CHANNEL     ADC1_CHANNEL_2
#define ADC_ATTEN       ADC_ATTEN_DB_12
#define ADC_WIDTH       ADC_WIDTH_BIT_12

static esp_adc_cal_characteristics_t adc1_chars;
bool cali_enable = false;

static bool adc_calibration_init() {
    esp_err_t ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret == ESP_ERR_NOT_SUPPORTED) return false;
    if (ret == ESP_ERR_INVALID_VERSION) return false;
    if (ret == ESP_OK) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 0, &adc1_chars);
        return true;
    }
    return false;
}

void setupADC() {
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    cali_enable = adc_calibration_init();
}
```
- `BAT_PIN`, `ADC_CHANNEL`, `ADC_ATTEN`, `ADC_WIDTH`: 定义了ADC的配置参数，包括连接引脚、ADC通道、衰减和位宽。
- `esp_adc_cal_characteristics_t adc1_chars`: 存储ADC校准数据的结构体。
- `cali_enable`: 布尔标志，指示ADC校准是否成功启用。
- `adc_calibration_init()`: 尝试初始化ADC校准。它会检查eFuse中的校准数据，如果存在且有效，则进行校准。
- `setupADC()`: 配置ADC的位宽和通道衰减，并尝试进行校准。

### 4.2 ADC读取与数据处理任务 `ADC_Task()`

```c++
void ADC_Task(void *pvParameters) {
    volts.analogMeter(0, 0, 3.3f, "V", "0", "0.8", "1.6", "2.4", "3.3");

    const float R_FIXED = 20000.0f;
    const float R10 = 8000.0f;
    const float GAMMA = 0.6f;

    while (!stopADCTask) {
        uint32_t sum = 0;
        const int samples = 50;
        for (int i = 0; i < samples; i++) {
            sum += adc1_get_raw(ADC_CHANNEL);
            delay(1);
        }
        sum /= samples;

        float voltage_v = 0;
        if (cali_enable) {
            uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(sum, &adc1_chars);
            voltage_v = voltage_mv / 1000.0f;
        } else {
            voltage_v = (sum * 3.3) / 4095.0;
        }
        volts.updateNeedle(voltage_v, 0);

        // --- Flicker-free display updates using sprite ---
        menuSprite.fillSprite(TFT_BLACK);
        menuSprite.setTextSize(2);
        menuSprite.setTextColor(TFT_WHITE, TFT_BLACK);

        // Calculate and display Lux
        float r_photo = (voltage_v * R_FIXED) / (3.3f - voltage_v);
        float lux = pow((r_photo / R10), (1.0f / -GAMMA)) * 10.0f;

        char luxStr[10];
        dtostrf(lux, 4, 1, luxStr);
        menuSprite.setCursor(20, 10);
        menuSprite.print("LUX: "); menuSprite.print(luxStr);

        // Display Voltage and ADC Raw Value
        char voltStr[10];
        dtostrf(voltage_v, 4, 2, voltStr);
        menuSprite.setCursor(20, 35);
        menuSprite.print("VOL: "); menuSprite.print(voltStr);

        menuSprite.setCursor(20, 60);
        menuSprite.print("ADC: "); menuSprite.print(sum);

        // Draw Progress Bar
        float constrainedLux = constrain(lux, 0.0f, 1000.0f); //
        int barWidth = map((long)constrainedLux, 0L, 100L, 0L, 200L); 
        menuSprite.drawRect(20, 85, 202, 22, TFT_WHITE);
        menuSprite.fillRect(21, 86, 200, 20, TFT_BLACK);
        menuSprite.fillRect(21, 86, barWidth, 20, TFT_GREEN);

        menuSprite.pushSprite(0, 130);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}
```
- 这是一个FreeRTOS任务，负责周期性地读取ADC值并更新UI。
- **模拟仪表**: 使用 `volts.analogMeter()` 初始化一个模拟仪表盘，并使用 `volts.updateNeedle()` 更新指针位置。
- **多样本平均**: 循环读取50个ADC原始值并计算平均值，以提高测量稳定性。
- **电压转换**: 根据 `cali_enable` 标志，使用校准函数 `esp_adc_cal_raw_to_voltage` 或线性映射将原始ADC值转换为电压。
- **光照度计算**: 根据光敏电阻的特性公式，将电压转换为光照度（Lux）。
- **UI显示**: 
    - 使用 `menuSprite` 进行双缓冲绘制，避免闪烁。
    - 显示计算出的Lux值、电压值和原始ADC值。
    - 绘制一个光照度进度条。
- **任务停止**: 循环会检查 `stopADCTask` 标志。当此标志为 `true` 时，任务会自行删除。

### 4.3 主ADC菜单函数 `ADCMenu()`

```c++
void ADCMenu() {
    stopADCTask = false;
    tft.fillScreen(TFT_BLACK);
    
    xTaskCreatePinnedToCore(ADC_Task, "ADC_Task", 4096, NULL, 1, NULL, 0);

    while (1) {
        if (exitSubMenu) {
            exitSubMenu = false; // Reset flag
            stopADCTask = true; // Signal the background task to stop
            vTaskDelay(pdMS_TO_TICKS(200)); // Wait for task to stop
            break;
        }
        if (g_alarm_is_ringing) { // ADDED LINE
            stopADCTask = true; // Signal the background task to stop
            vTaskDelay(pdMS_TO_TICKS(200)); // Give task time to stop
            break; // Exit loop
        }
        if (readButton()) {
            stopADCTask = true;
            vTaskDelay(pdMS_TO_TICKS(200)); 
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}
```
- `ADCMenu()` 是ADC模块的主入口函数。
- **初始化**: 清屏，重置 `stopADCTask` 标志。
- **创建任务**: 创建 `ADC_Task` 任务来处理ADC读取和UI更新。
- **主循环**: 持续运行，直到满足退出条件。
- **退出条件**: 
    - `exitSubMenu`: 由主菜单触发的退出标志。
    - `g_alarm_is_ringing`: 闹钟响铃时强制退出。
    - `readButton()`: 用户单击按键退出。
- **任务停止**: 退出时，将 `stopADCTask` 设置为 `true`，并等待 `ADC_Task` 自行删除，确保资源被正确释放。

## 五、总结与展望

ADC模块成功地实现了模拟信号的读取、校准和转换为实际物理量（光照度），并通过直观的图形化界面展示。其多样本平均和ADC校准功能提高了测量的准确性和稳定性。

未来的改进方向：
1.  **多通道支持**: 允许同时读取多个ADC通道，并显示不同传感器的值。
2.  **传感器类型配置**: 允许用户配置连接的传感器类型（例如，光敏电阻、电位器、热敏电阻），并根据类型应用不同的转换公式。
3.  **校准过程**: 提供用户友好的校准过程，例如通过UI引导用户进行两点校准。
4.  **数据记录**: 将ADC数据记录到SD卡或通过网络发送到服务器，方便长期监测。
5.  **警报功能**: 当ADC值或计算出的物理量超出阈值时，触发视觉或声音警报。

谢谢大家
