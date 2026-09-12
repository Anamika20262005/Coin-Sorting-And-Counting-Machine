#include <Wire.h> 
#include <Adafruit_PWMServoDriver.h> 
#include "HX711.h" 
#include <LiquidCrystal_I2C.h> 
/* * COIN SORTER SYSTEM - FINAL CONFIGURATION 
* 10 Rs -> 14 deg | 20 Rs -> 43 deg | 5 Rs -> 77 deg 
* 2 Rs  -> 107 deg | 1 Rs  -> 140 deg | Unknown -> 170 deg 
*/ 
// ---------------- LCD & PCA9685 ---------------- 
LiquidCrystal_I2C lcd(0x27, 16, 2); 
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40); 
// ---------------- Pin Assignments -------------- 
#define SENSOR_PIN 13  // Inductive/Proximity Sensor 
#define DOUT 2         
// HX711 Data 
#define CLK  12        // HX711 Clock 
// ---------------- Servo Settings --------------- 
#define SERVOMIN 110 
#define SERVOMAX 500 
int servoCont = 0;     // Continuous Feeder 
int servoPush = 1;     // Load Cell Pusher 
int servoSort = 2;     // Sorting Chute 
int servoBin  = 3;     // Bin Ejector 
#define BIN_MIN 500 
#define BIN_MAX 110 
#define SERVO_SLOW_CW 320   
// ---------------- Weight & Data ---------------- 
HX711 scale; 
f
loat calibration_factor = 7050;  
f
loat weightThreshold = 0.2; 
#define MAX_RETRY 3 
struct Coin { 
String label; 
int angle; 
int count; 
int value; 
}; 
Coin coinTable[] = { 
{"10 Rs", 14,  0, 10}, 
{"20 Rs", 43,  0, 20}, 
{"5 Rs",  77,  0, 5}, 
{"2 Rs",  107, 0, 2}, 
{"1 Rs",  140, 0, 1} 
}; 
f
loat totalAmount = 0; 
int countUnknown = 0; 
bool coinOnLoadCell = false; 
unsigned long lastCoinTime = 0; 
const unsigned long noCoinDelay = 10000; // 10s Summary Trigger 
// ---------------- Functions -------------------- 
void clearData() { 
totalAmount = 0; 
countUnknown = 0; 
for (int i = 0; i < 5; i++) coinTable[i].count = 0; 
scale.tare(); 
lcd.clear(); 
lcd.print("Data Cleared"); 
lcd.setCursor(0,1); 
lcd.print("Ready for Next"); 
delay(1500); 
} 
int angleToPulse(int angle) { 
return map(angle, 0, 180, 110, 510); 
} 
f
loat getAverage() { 
f
loat totalAvgSum = 0; 
for (int setCount = 0; setCount < 5; setCount++) { 
f
loat setSum = 0; 
for (int sampleCount = 0; sampleCount < 10; sampleCount++) { 
setSum += scale.get_units(1); 
delay(50); 
} 
totalAvgSum += (setSum / 10.0); 
} 
return totalAvgSum / 5.0; 
} 
// ---------------- SETUP ------------------------ 
void setup() { 
Serial.begin(115200); 
delay(1000); // Startup delay for Power Supply stability 
Wire.begin(14, 15); 
lcd.init(); 
lcd.backlight(); 
lcd.print("System Loading"); 
pwm.begin(); 
pwm.setPWMFreq(50); 
pinMode(SENSOR_PIN, INPUT_PULLUP); 
scale.begin(DOUT, CLK); 
scale.set_scale(calibration_factor); 
// Staggered initialization to prevent current surge 
pwm.setPWM(servoPush, 0, SERVOMIN); 
delay(400); 
pwm.setPWM(servoSort, 0, angleToPulse(90)); 
delay(400); 
pwm.setPWM(servoBin, 0, BIN_MIN); 
delay(400); 
scale.tare(); 
lcd.clear(); 
lcd.print("System Ready"); 
lastCoinTime = millis(); 
} 
// ---------------- LOOP ------------------------- 
void loop() { 
pwm.setPWM(servoCont, 0, SERVO_SLOW_CW); 
// 1. Detection Phase 
if (digitalRead(SENSOR_PIN) == LOW && !coinOnLoadCell) { 
delay(200); // Inductive Signal Delay 
pwm.setPWM(servoPush, 0, SERVOMAX); 
delay(500); 
    pwm.setPWM(servoPush, 0, SERVOMIN); 
    delay(500); 
    coinOnLoadCell = true; 
    lastCoinTime = millis();  
  } 
 
  // 2. Weighing & Sorting Phase 
  if (coinOnLoadCell && scale.is_ready()) { 
    float reading = scale.get_units(1); 
 
if (reading > weightThreshold) { 
      delay(1000); // Settle time 
      int foundIdx = -1; 
 
      for (int attempt = 1; attempt <= MAX_RETRY; attempt++) { 
        float finalAvg = getAverage(); 
 
        if (finalAvg >= 1.00 && finalAvg <= 1.09) foundIdx = 0;  
        else if (finalAvg >= 1.10 && finalAvg <= 1.21) foundIdx = 1;  
        else if ( 
          (finalAvg >= 0.88 && finalAvg <= 0.99) || (finalAvg >= 0.80 && finalAvg <= 0.86) || 
          (finalAvg >= 0.63 && finalAvg <= 0.66) || (finalAvg >= 1.20 && finalAvg <= 1023) || 
          (finalAvg >= 1.26 && finalAvg <= 1.27) 
        ) foundIdx = 2;  
        else if ( 
          (finalAvg >= 0.53 && finalAvg <= 0.57) || (finalAvg >= 0.63 && finalAvg <= 0.64) || 
          (finalAvg >= 0.74 && finalAvg <= 0.75) || (finalAvg >= 0.70 && finalAvg <= 0.71) || 
          (finalAvg >= 0.74 && finalAvg <= 0.79) || (finalAvg >= 0.85 && finalAvg <= 0.86) 
        ) foundIdx = 3;  
        else if ( 
          (finalAvg >= 0.00 && finalAvg <= 0.52) || (finalAvg >= 0.59 && finalAvg <= 0.60) || 
          (finalAvg >= 0.64 && finalAvg <= 0.69) || (finalAvg >= 0.69 && finalAvg <= 0.72) || 
(finalAvg >= 0.74 && finalAvg <= 0.76) 
        ) foundIdx = 4;  
 
        if (foundIdx != -1) break; 
        else delay(500); 
      } 
 
      String dName = (foundIdx != -1) ? coinTable[foundIdx].label : "Unknown"; 
      int dAngle = (foundIdx != -1) ? coinTable[foundIdx].angle : 170; 
 
      if (foundIdx != -1) { 
        coinTable[foundIdx].count++; 
        totalAmount += coinTable[foundIdx].value; 
      } else { 
        countUnknown++; 
      } 
 
      lcd.clear(); 
      lcd.print(dName); 
      lcd.setCursor(0, 1); 
      lcd.print("Sorting..."); 
 
      pwm.setPWM(servoSort, 0, angleToPulse(dAngle)); 
      delay(800); 
      pwm.setPWM(servoBin, 0, BIN_MAX); 
      delay(400); 
      pwm.setPWM(servoBin, 0, BIN_MIN); 
      pwm.setPWM(servoSort, 0, angleToPulse(90)); 
       
      coinOnLoadCell = false; 
      lastCoinTime = millis(); 
    } 
  } 
 
  // 3. Summary & Automated Reset Phase 
  if (!coinOnLoadCell && (millis() - lastCoinTime > noCoinDelay) && totalAmount > 0) { 
    pwm.setPWM(servoCont, 0, 0);  
 
    for (int i = 0; i < 5; i++) { 
      if (coinTable[i].count > 0) { 
        lcd.clear(); 
        lcd.print(coinTable[i].label + " x" + String(coinTable[i].count)); 
        lcd.setCursor(0, 1); 
        lcd.print("Sum: Rs " + String(coinTable[i].count * coinTable[i].value)); 
        delay(2500);  
      } 
} 
lcd.clear(); 
lcd.print("GRAND TOTAL"); 
lcd.setCursor(0, 1); 
lcd.print("Rs: " + String(total Amount)); 
delay (5000); 
clear Data(); 
last Coin  Time = millis();
}
}

-