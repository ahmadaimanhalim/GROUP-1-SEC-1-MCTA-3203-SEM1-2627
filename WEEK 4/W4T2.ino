#include <Wire.h>
#include <MPU6050.h>
#include <Servo.h>

MPU6050 mpu;
Servo servo;
int servoPosition = 90;

void setup() {
  servo.attach(9);
  servo.write(servoPosition); // Start in locked position (90 deg)
  
  pinMode(11, OUTPUT);  // Green LED
  pinMode(10, OUTPUT);  // Red LED
  
  // Start with Red LED ON and Green LED OFF
  digitalWrite(11, LOW);
  digitalWrite(10, HIGH);
  
  Serial.begin(9600);
  Wire.begin();
  mpu.initialize();
}

void loop() {
  // 1. Check for 'A' (Unlock) or 'D' (Lock) from Python
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'A') {
      servo.write(180);       // Unlock servo
      digitalWrite(11, HIGH); // Green LED ON
      digitalWrite(10, LOW);  // Red LED OFF
    } else if (command == 'D') {
      servo.write(90);        // Lock servo
      digitalWrite(11, LOW);  // Green LED OFF
      digitalWrite(10, HIGH); // Red LED ON
    }
  }

  // 2. Send ax,ay,az,gx,gy,gz to Python for the circular motion check
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  Serial.print(ax); Serial.print(",");
  Serial.print(ay); Serial.print(",");
  Serial.print(az); Serial.print(",");
  Serial.print(gx); Serial.print(",");
  Serial.print(gy); Serial.print(",");
  Serial.println(gz);
  
  delay(50);
}