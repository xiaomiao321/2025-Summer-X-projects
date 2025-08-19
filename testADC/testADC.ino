#define ADC_PIN 2  // GPIO2作为ADC引脚

void setup() {
  Serial.begin(115200);              // 串口初始化，用于输出电压
  analogReadResolution(12);          // 设置ADC分辨率为12位（0-4095）
  Serial.println("ADC GPIO2电压读取测试程序启动（光敏电阻）");  // 输出中文提示
}

void loop() {
  int adcValue = analogRead(ADC_PIN);  // 读取ADC值
  float voltage = (adcValue / 4095.0) * 3.3;  // 计算电压（参考3.3V）

  Serial.print("ADC值: ");
  Serial.print(adcValue);            // 输出ADC原始值
  Serial.print(" | 电压: ");
  Serial.print(voltage, 2);          // 输出电压（保留2位小数）
  Serial.println(" V");              // 单位V

  delay(1000);                       // 每秒读取一次
}