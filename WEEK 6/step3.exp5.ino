const int EN1  = 5; // PWM pin for Motor 1 speed control
const int DIR1 = 4; // Digital pin for Motor 1 direction control

void setup() {
  pinMode(EN1, OUTPUT);
  pinMode(DIR1, OUTPUT);
  Serial.begin(9600);

  // Set forward rotation direction
  digitalWrite(DIR1, HIGH);
}

void loop() {
  // Increment PWM from 0 to 255 in steps of 5
  for (int pwmValue = 0; pwmValue <= 255; pwmValue += 5) {
    analogWrite(EN1, pwmValue);
    Serial.print("Current PWM Value: ");
    Serial.println(pwmValue);
    delay(1000); // Hold 1 second per step to observe shaft startup
  }

  // Stop motor for 5 seconds before restarting sweep
  analogWrite(EN1, 0);
  Serial.println("Sweep complete. Motor stopped.");
  delay(5000);
}