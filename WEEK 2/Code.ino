// Define the pins for each segment based on your configuration
const int segmentA = 3;
const int segmentB = 2;
const int segmentC = 8;
const int segmentD = 7;
const int segmentE = 6;
const int segmentF = 4;
const int segmentG = 5;

// Define the pushbutton pins
const int incrementBtn = 12;  //[cite: 3]
const int resetBtn = 13;     //[cite: 3]

// Variables for state tracking
int count = 0;
int lastIncState = HIGH;
int lastResState = HIGH;

void setup() {
  pinMode(segmentA, OUTPUT);
  pinMode(segmentB, OUTPUT);
  pinMode(segmentC, OUTPUT);
  pinMode(segmentD, OUTPUT);
  pinMode(segmentE, OUTPUT);
  pinMode(segmentF, OUTPUT);
  pinMode(segmentG, OUTPUT);

  pinMode(incrementBtn, INPUT); 
  pinMode(resetBtn, INPUT);     
}

void loop() {
  int currentIncState = digitalRead(incrementBtn);
  int currentResState = digitalRead(resetBtn);

  // Increment button logic
  if (currentIncState == LOW && lastIncState == HIGH) {
    count++;
    if (count > 9) { 
      count = 0; // Sequence resets after 9[cite: 3]
    }
    delay(50); // Debounce
  }

  // Reset button logic
  if (currentResState == LOW && lastResState == HIGH) {
    count = 0;
    delay(50); // Debounce
  }

  lastIncState = currentIncState;
  lastResState = currentResState;

  // Update the 7-segment display based on the current count
  displayNumber(count);
}

// Function to handle the segment logic for each number
void displayNumber(int num) {
  // First, turn off all segments
  digitalWrite(segmentA, LOW); digitalWrite(segmentB, LOW);
  digitalWrite(segmentC, LOW); digitalWrite(segmentD, LOW);
  digitalWrite(segmentE, LOW); digitalWrite(segmentF, LOW);
  digitalWrite(segmentG, LOW);

  // Then, turn on the specific segments for the current number
  switch (num) {
    case 0: // A,B,C,D,E,F
      digitalWrite(segmentA, HIGH); digitalWrite(segmentB, HIGH);
      digitalWrite(segmentC, HIGH); digitalWrite(segmentD, HIGH);
      digitalWrite(segmentE, HIGH); digitalWrite(segmentF, HIGH);
      break;
    case 1: // B,C
      digitalWrite(segmentB, HIGH); digitalWrite(segmentC, HIGH);
      break;
    case 2: // A,B,G,E,D
      digitalWrite(segmentA, HIGH); digitalWrite(segmentB, HIGH);
      digitalWrite(segmentG, HIGH); digitalWrite(segmentE, HIGH);
      digitalWrite(segmentD, HIGH);
      break;
    case 3: // A,B,C,D,G
      digitalWrite(segmentA, HIGH); digitalWrite(segmentB, HIGH);
      digitalWrite(segmentC, HIGH); digitalWrite(segmentD, HIGH);
      digitalWrite(segmentG, HIGH);
      break;
    case 4: // B,C,F,G
      digitalWrite(segmentB, HIGH); digitalWrite(segmentC, HIGH);
      digitalWrite(segmentF, HIGH); digitalWrite(segmentG, HIGH);
      break;
    case 5: // A,C,D,F,G
      digitalWrite(segmentA, HIGH); digitalWrite(segmentC, HIGH);
      digitalWrite(segmentD, HIGH); digitalWrite(segmentF, HIGH);
      digitalWrite(segmentG, HIGH); 
      break;
    case 6: // A,C,D,E,F,G
      digitalWrite(segmentA, HIGH); digitalWrite(segmentC, HIGH);
      digitalWrite(segmentD, HIGH); digitalWrite(segmentE, HIGH);
      digitalWrite(segmentF, HIGH); digitalWrite(segmentG, HIGH);
      break;
    case 7: // A,B,C
      digitalWrite(segmentA, HIGH); digitalWrite(segmentB, HIGH);
      digitalWrite(segmentC, HIGH); 
      break;
    case 8: // A,B,C,D,E,F,G
      digitalWrite(segmentA, HIGH); digitalWrite(segmentB, HIGH);
      digitalWrite(segmentC, HIGH); digitalWrite(segmentD, HIGH);
      digitalWrite(segmentE, HIGH); digitalWrite(segmentF, HIGH);
      digitalWrite(segmentG, HIGH);
      break;
    case 9: // A,B,C,D,F,G
      digitalWrite(segmentA, HIGH); digitalWrite(segmentB, HIGH);
      digitalWrite(segmentC, HIGH); digitalWrite(segmentD, HIGH);
      digitalWrite(segmentF, HIGH); digitalWrite(segmentG, HIGH);
      break;
  }
}
