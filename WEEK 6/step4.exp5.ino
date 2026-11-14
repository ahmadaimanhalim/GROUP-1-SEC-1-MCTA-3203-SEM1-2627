const int EN1  = 5; // PWM pin for speed control
const int DIR1 = 4; // Digital pin for direction control

void setup() {
  pinMode(EN1, OUTPUT);
  pinMode(DIR1, OUTPUT);
}

void loop() {
  // --- FORWARD DIRECTION ---
  digitalWrite(DIR1, HIGH);

  analogWrite(EN1, 255); // Forward: PWM 255
  delay(5000);

  analogWrite(EN1, 150); // Forward: PWM 150
  delay(5000);

  analogWrite(EN1, 75);  // Forward: PWM 75
  delay(5000);

  // Brief stop before reversing
  analogWrite(EN1, 0);
  delay(1000);

  // --- REVERSE DIRECTION ---
  digitalWrite(DIR1, LOW);

  analogWrite(EN1, 255); // Reverse: PWM 255
  delay(5000);

  analogWrite(EN1, 150); // Reverse: PWM 150
  delay(5000);

  analogWrite(EN1, 75);  // Reverse: PWM 75
  delay(5000);

  // Brief stop before repeating loop
  analogWrite(EN1, 0);
  delay(1000);
}