const int EN1  = 5; // Left Motor PWM (Motor 1)
const int DIR1 = 4; // Left Motor Direction
const int EN2  = 6; // Right Motor PWM (Motor 2)
const int DIR2 = 7; // Right Motor Direction

const int FAST     = 255;
const int MODERATE = 150;
const int SLOW     = 75;

void setup() {
  pinMode(EN1, OUTPUT);
  pinMode(DIR1, OUTPUT);
  pinMode(EN2, OUTPUT);
  pinMode(DIR2, OUTPUT);
}

void driveRobot(int speedLeft, int speedRight) {
  digitalWrite(DIR1, (speedLeft >= 0) ? HIGH : LOW);
  digitalWrite(DIR2, (speedRight >= 0) ? HIGH : LOW);

  analogWrite(EN1, constrain(abs(speedLeft), 0, 255));
  analogWrite(EN2, constrain(abs(speedRight), 0, 255));
}

void loop() {
  driveRobot(FAST, FAST);           // Straight forward
  delay(2000);

  driveRobot(FAST, SLOW);           // Curve right
  delay(2000);

  driveRobot(SLOW, FAST);           // Curve right
  delay(2000);

  driveRobot(MODERATE, -MODERATE);  // Pivot turn right
  delay(1500);

  driveRobot(-MODERATE, MODERATE);  // Pivot turn left
  delay(1500);

  driveRobot(-MODERATE, -MODERATE); // Straight reverse
  delay(2000);

  driveRobot(0, 0);                 // Stop
  delay(2000);
}