#include <OneWire.h>              // OneWire库，用于单总线通信
#include <DallasTemperature.h>    // DallasTemperature库，用于DS18B20

#define DS18B20_PIN 10            // GPIO10作为DS18B20数据引脚

OneWire oneWire(DS18B20_PIN);     // 初始化OneWire对象
DallasTemperature sensors(&oneWire);  // 初始化传感器对象

void setup() {
  Serial.begin(115200);           // 串口初始化，用于输出温度
  sensors.begin();                // 启动传感器
  Serial.println("DS18B20温度传感器测试程序启动");  // 输出中文提示
}

void loop() {
  sensors.requestTemperatures();  // 请求温度读取
  float tempC = sensors.getTempCByIndex(0);  // 获取第一个传感器的摄氏温度

  if (tempC != DEVICE_DISCONNECTED_C) {  // 检查是否连接
    Serial.print("当前温度: ");
    Serial.print(tempC);
    Serial.println(" °C");        // 输出温度
  } else {
    Serial.println("传感器未连接或错误");  // 错误提示
  }

  delay(1000);                    // 每秒读取一次
}