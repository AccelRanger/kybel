#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// PINS

int ENA1 = 12;
int ENA2 = 11;
int ENB1 = 10;
int ENB2 = 9;

int VENT_RELAY = 2;

int FL_SENS = A3;
int FR_SENS = A4;
int LF_SENS = A5;
int RT_SENS = A6;

int DS_TRIG = 3;
int DS_ECHO = 4;

int flameLeftPin = A0 ;
int flameCenterPin = A1;
int flameRightPin = A2;

int forwardSpeed = 130;
int turnSpeed = 120;

int G1 = 22;
int G2 = 23;
int G3 = 24;
int G4 = 25;

// STATE MACHINE

enum StateID {
  FIND_CANDLE,
  APPROACH,
  FIND_FIRE,
  KILL_FIRE,
  CALIBRATE,
  IDLE,
  ERR,
  NUM_STATES
};

StateID currentState = IDLE;
StateID nextState = IDLE;

typedef void (*StateFunction)();

struct State {
  StateFunction onEnter;
  StateFunction onUpdate;
  StateFunction onExit;
};

// COMPILER INFORMATION // DO NOT WRITE CODE //
void findCandleEnter();
void findCandleUpdate();
void findCandleExit();
void approachEnter();
void approachUpdate();
void approachExit();
void findFireEnter();
void findFireUpdate();
void findFireExit();
void killFireEnter();
void killFireUpdate();
void killFireExit();
void calibrateEnter();
void calibrateUpdate();
void calibrateExit();
void idleEnter();
void idleUpdate();
void idleExit();
void errEnter();
void errUpdate();
void errExit();

// STATES

State states[NUM_STATES] = {
  { findCandleEnter, findCandleUpdate, findCandleExit },
  { approachEnter, approachUpdate, approachExit },
  { findFireEnter, findFireUpdate, findFireExit },
  { killFireEnter, killFireUpdate, killFireExit },
  { calibrateEnter, calibrateUpdate, calibrateExit },
  { idleEnter, idleUpdate, idleExit },
  { errEnter, errUpdate, errExit }
};

// FUNC

void displayText(String line1, String line2) {
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// MAIN

void setup() {
  Serial.begin(9600);

  lcd.init();          // Initialize the LCD
  lcd.backlight();     // Turn on the backlight

  displayText("KYBEL MAIN MENU", "ERC_1 / NO_CALIB");

  pinMode(ENA1, OUTPUT);
  pinMode(ENA2, OUTPUT);
  pinMode(ENB1, OUTPUT);
  pinMode(ENB2, OUTPUT);

  pinMode(G1, OUTPUT);
  pinMode(G2, OUTPUT);
  pinMode(G3, OUTPUT);
  pinMode(G4, OUTPUT);

  pinMode(VENT_RELAY, OUTPUT);

  states[currentState].onEnter(); // STATE MACHINE

  digitalWrite(ENA1, LOW);
  digitalWrite(ENA2, LOW);
  digitalWrite(ENB1, LOW);
  digitalWrite(ENB2, LOW);
}

void loop() {
  states[currentState].onUpdate();

  if (nextState != currentState) {
    states[currentState].onExit();
    currentState = nextState;
    states[currentState].onEnter();
  }

  delay(50);
}

// MOTOR FUNCTIONS

void stopMotors() {
  analogWrite(ENA1, 0);
  analogWrite(ENA2, 0);
  analogWrite(ENB1, 0);
  analogWrite(ENB2, 0);
}

void forward() {
  analogWrite(ENA1, forwardSpeed);
  analogWrite(ENA2, 0);
  analogWrite(ENB1, 0);
  analogWrite(ENB2, forwardSpeed);
}

void backward() {
  analogWrite(ENA1, 0);
  analogWrite(ENA2, forwardSpeed);
  analogWrite(ENB1, forwardSpeed);
  analogWrite(ENB2, 0);
}

void turnRight() {
  analogWrite(ENA1, 0);
  analogWrite(ENA2, turnSpeed);
  analogWrite(ENB1, 0);
  analogWrite(ENB2, turnSpeed);
}

void turnLeft() {
  analogWrite(ENA1, turnSpeed);
  analogWrite(ENA2, 0);
  analogWrite(ENB1, turnSpeed);
  analogWrite(ENB2, 0);
}

long getDistanceRaw() {
  digitalWrite(DS_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(DS_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(DS_TRIG, LOW);

  long duration = pulseIn(DS_ECHO, HIGH, 25000);
  if (duration == 0) return 999999;
  return duration;
}

bool candleDetected() {
  long duration;

  digitalWrite(DS_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(DS_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(DS_TRIG, LOW);

  duration = pulseIn(DS_ECHO, HIGH, 25000);
  if (duration == 0) return false;

  float distanceM = (duration * 0.000343) / 2.0;

  Serial.print("Dist: ");
  Serial.print(distanceM);
  Serial.println(" m");

  return (distanceM > 0 && distanceM <= 0.4);
}

// CHANGING STATE >> nextState = OTHER STATE!!; / REST HANDLED IN LOOP

// STATE DEFINITIONS // CODE HERE //

// FIND_CANDLE
unsigned long scanStartTime;
unsigned long forwardStartTime;
bool doingForward = false;
bool didTurnRight = false;

void findCandleEnter() {
  displayText("KYBEL > RUNNING", "STATE: FIND_CANDLE");
  Serial.println("Entering FIND_CANDLE");
  stopMotors();
}

void findCandleUpdate() {
  Serial.println("SCAN: Left pulses (10x)");
  for (int i = 0; i < 10; i++) {
    turnLeft();
    delay(50);
    stopMotors();
    delay(100);

    if (candleDetected()) {
      Serial.println("Candle found LEFT SCAN!");
      nextState = APPROACH;
      return;
    }
  }

  Serial.println("SCAN: Right pulses (20x)");
  for (int i = 0; i < 20; i++) {
    turnRight();
    delay(50);
    stopMotors();
    delay(100);

    if (candleDetected()) {
      Serial.println("Candle found RIGHT SCAN!");
      nextState = APPROACH;
      return;
    }
  }

  Serial.println("SCAN: Left pulses again (10x)");
  for (int i = 0; i < 10; i++) {
    turnLeft();
    delay(50);
    stopMotors();
    delay(100);

    if (candleDetected()) {
      Serial.println("Candle found SECOND LEFT SCAN!");
      nextState = APPROACH;
      return;
    }
  }

  Serial.println("Nothing found. Moving forward...");

  for (int i = 0; i < 10; i++) {
    forward();
    delay(100);
    stopMotors();
    delay(100);
  }
}

void findCandleExit() {
  stopMotors();
  Serial.println("Exiting FIND_CANDLE");
}

// =================== APPROACH ======================== //

void approachEnter() {
  displayText("KYBEL > RUNNING", "STATE: APPROACH");
  Serial.println("Entering APPROACH");
  stopMotors();
}

void approachUpdate() {
  long distRaw = getDistanceRaw();
  float dist = (distRaw * 0.000343) / 2.0;

  // ====== READ LINE SENSORS ======
  int fl = analogRead(FL_SENS);
  int fr = analogRead(FR_SENS);
  int lf = analogRead(LF_SENS);
  int rt = analogRead(RT_SENS);

  bool seesBlack = false;//(fl < 400 || fr < 400 || lf < 400 || rt < 400);

  // ====== ARENA BOUNDARY PROTECTION ======
  if (seesBlack) {
    Serial.println("BLACK detected! Checking boundary...");

    // If strong boundary detected (arena), stop + reverse + turn away
    if ((lf < 400 && rt < 400) || (fl < 400 && fr < 400)) {
      Serial.println("ARENA BORDER - BACKING UP!");
      backward();
      delay(600);
      stopMotors();
      turnLeft();
      delay(500);
      stopMotors();
      return;
    }
  }

  // ====== APPROACH TARGET ======
  if (dist > 0.05 && dist <= 0.40) {

    if (dist > 0.25) {
      // safe far approach
      Serial.println("Approaching fast...");
      forward();
    } 
    else if (dist > 0.18) {
      // slower approach
      Serial.println("Approaching slow...");
      analogWrite(ENA1, 80);
      analogWrite(ENA2, 0);
      analogWrite(ENB1, 0);
      analogWrite(ENB2, 80);
    } 
    else {
      // close to object
      Serial.println("Close to object. Checking...");
      stopMotors();
      delay(200);

      // ====== WALL CHECK ======
      // rotate slightly left
      turnLeft();
      delay(200);
      stopMotors();
      long leftCheck = getDistanceRaw();
      float leftDist = (leftCheck * 0.000343) / 2.0;

      // rotate slightly right
      turnRight();
      delay(400);
      stopMotors();
      long rightCheck = getDistanceRaw();
      float rightDist = (rightCheck * 0.000343) / 2.0;

      // realign forward
      turnLeft();
      delay(200);
      stopMotors();

      // ====== WALL DETECTED: navigate around ======
      if (leftDist < 0.40 && rightDist < 0.40) {
        Serial.println("Wall in front! Navigating around...");
        turnRight();
        delay(600);
        stopMotors();
        forward();
        delay(400);
        stopMotors();
        return;
      }

      // ====== NO WALL: this is the candle circle ======
      Serial.println("Object confirmed as candle circle!");
      nextState = FIND_FIRE;
      return;
    }
  } 
  else {
    // Lost target → return to scanning
    Serial.println("Lost object, returning to FIND_CANDLE");
    stopMotors();
    nextState = FIND_CANDLE;
  }
}

void approachExit() {
  stopMotors();
  Serial.println("Exiting APPROACH");
}

// FIND_FIRE
void findFireEnter() {}
void findFireUpdate() {}
void findFireExit() {}

// KILL_FIRE
void killFireEnter() {
  displayText("KYBEL > RUNNING", "STATE: SEEK AND DESTROY");
 // digitalWrite(VENT_RELAY, HIGH);
}
void killFireUpdate() {
  delay(1000);
}
void killFireExit() {
 // digitalWrite(VENT_RELAY, LOW);
}

// CALIBRATE
int d = 5;

void calibrateEnter() {
   displayText("KYBEL > CALIBRATING", "STATE: CALIBRATE");
}
void calibrateUpdate() {
  for (int i = 0; i < 200; i++) {
    digitalWrite(G1, HIGH); digitalWrite(G2, LOW);
    digitalWrite(G3, HIGH); digitalWrite(G4, LOW);
    delay(d);

    digitalWrite(G1, LOW); digitalWrite(G2, HIGH);
    digitalWrite(G3, HIGH); digitalWrite(G4, LOW);
    delay(d);

    digitalWrite(G1, LOW); digitalWrite(G2, HIGH);
    digitalWrite(G3, LOW); digitalWrite(G4, HIGH);
    delay(d);

    digitalWrite(G1, HIGH); digitalWrite(G2, LOW);
    digitalWrite(G3, LOW); digitalWrite(G4, HIGH);
    delay(d);
  }

  delay(1000); // pause

  for (int i = 0; i < 200; i++) {
    digitalWrite(G1, HIGH); digitalWrite(G2, LOW);
    digitalWrite(G3, LOW); digitalWrite(G4, HIGH);
    delay(d);

    digitalWrite(G1, LOW); digitalWrite(G2, HIGH);
    digitalWrite(G3, LOW); digitalWrite(G4, HIGH);
    delay(d);

    digitalWrite(G1, LOW); digitalWrite(G2, HIGH);
    digitalWrite(G3, HIGH); digitalWrite(G4, LOW);
    delay(d);

    digitalWrite(G1, HIGH); digitalWrite(G2, LOW);
    digitalWrite(G3, HIGH); digitalWrite(G4, LOW);
    delay(d);
  }

}
void calibrateExit() {}

// IDLE
void idleEnter() {
  Serial.println("Hello from IDLE!");
  nextState = FIND_CANDLE;
}
void idleUpdate() {
  //turnLeft();
}
void idleExit() {}

// ERR
void errEnter() {
  Serial.println("Hello from ERR!");
}
void errUpdate() {}
void errExit() {}

// krakotina
