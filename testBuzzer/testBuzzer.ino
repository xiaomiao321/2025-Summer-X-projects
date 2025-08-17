const int beepPin = 5;
void setup() {
   pinMode(beepPin, OUTPUT);
}
void loop() {
   for (int i = 0; i < 1000; i++) { // 循环1000次，相当于生成500个占空比50%的方波
       if (i % 2 == 0) { // 对2取余，如果是0，则为偶数
           digitalWrite(beepPin, HIGH);
       } else {
           digitalWrite(beepPin, LOW);
       }
       delayMicroseconds(270); // 延迟270微秒
   }
}