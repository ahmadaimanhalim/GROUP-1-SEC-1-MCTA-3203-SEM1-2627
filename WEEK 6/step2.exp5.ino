const int EN1  = 5; // PWM pin for Motor 1 speed control (ENA)
const int DIR1 = 4; // Digital pin for Motor 1 direction control

void setup() {
  pinMode(EN1, OUTPUT);
  pinMode(DIR1, OUTPUT);
}

void loop() {
  // Move forward at PWM 100 for 2 seconds
  digitalWrite(DIR1, HIGH);
  analogWrite(EN1, 100);
  delay(2000);

  // Move backward at PWM 100 for 2 seconds
  digitalWrite(DIR1, LOW);
  analogWrite(EN1, 100);
  delay(2000);

  // Stop motor for 1 second
  analogWrite(EN1, 0);
  delay(1000);
}