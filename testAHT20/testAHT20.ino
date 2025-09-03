#include <Adafruit_AHTX0.h>
#include <Wire.h>
Adafruit_AHTX0 aht;
 
void setup() {
  //Wire.begin(I2C_SDA, I2C_SCL);
  Wire.begin(20, 21);
  Serial.begin(115200);
  Serial.println("Adafruit AHT10/AHT20 demo!");
 
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");
}
 
void loop() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  Serial.print("Temperature: "); 
  Serial.print(temp.temperature);
  Serial.println(" degrees C");
  Serial.print("Humidity: "); 
  Serial.print(humidity.relative_humidity); Serial.println("% rH");
 
  delay(500);
}