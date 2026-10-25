#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

unsigned long lastSampleMs = 0;
const unsigned long SAMPLE_INTERVAL_MS = 50;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  mpu.initialize();

  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  if (!mpu.testConnection()) {
    Serial.println("ERR: MPU6050 connection failed!");
    while (1);
  }
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;

    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    Serial.print(ax); Serial.print(",");
    Serial.print(ay); Serial.print(",");
    Serial.print(az); Serial.print(",");
    Serial.print(gx); Serial.print(",");
    Serial.print(gy); Serial.print(",");
    Serial.println(gz);
  }
}