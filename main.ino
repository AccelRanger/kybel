#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// THRESHOLDS

#define LINE_SENS_BLACK_THRESHOLD 200
#define LINE_SENS_WHITE_THRESHOLD 800

// DRIVER SPEEDS

#define STARTING_SPEED_FORWARD 130
#define SLOWING_SPEED_FORWARD 50

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

int forwardSpeed = 150;
int turnSpeed = 130;

int G1 = 22;
int G2 = 23;
int G3 = 24;
int G4 = 25;

int RESET_PIN = 5;

int stepd = 5; //delay for stepper

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

// COMPILER INFORMATION // STATE MACHINE FUNC PRESET //
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
  pinMode(ENA1, OUTPUT);
  pinMode(ENA2, OUTPUT);
  pinMode(ENB1, OUTPUT);
  pinMode(ENB2, OUTPUT);

  digitalWrite(ENA1, LOW);
  digitalWrite(ENA2, LOW);
  digitalWrite(ENB1, LOW);
  digitalWrite(ENB2, LOW);

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  displayText("KYBEL MAIN MENU", "ERC_1 / NO_CALIB");

  pinMode(G1, OUTPUT);
  pinMode(G2, OUTPUT);
  pinMode(G3, OUTPUT);
  pinMode(G4, OUTPUT);

  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);

  pinMode(VENT_RELAY, OUTPUT);

  states[currentState].onEnter(); // STATE MACHINE
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
  analogWrite(ENA1, STARTING_SPEED_FORWARD);
  analogWrite(ENA2, 0);
  analogWrite(ENB1, 0);
  analogWrite(ENB2, STARTING_SPEED_FORWARD);
 // delay(50);
 // analogWrite(ENA1, SLOWING_SPEED_FORWARD);
 // analogWrite(ENA2, 0);
 // analogWrite(ENB1, 0);
 // analogWrite(ENB2, SLOWING_SPEED_FORWARD);
}

void backward() {
  analogWrite(ENA1, 0);
  analogWrite(ENA2, forwardSpeed);
  analogWrite(ENB1, 0);
  analogWrite(ENB2, forwardSpeed);
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

// HCR04 US FUNC

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

  return (distanceM > 0 && distanceM <= 0.65);
}

//0-1023
bool isWhite(int val) { return val > LINE_SENS_WHITE_THRESHOLD; }
bool isBlack(int val) { return val < LINE_SENS_BLACK_THRESHOLD; }

bool insideCircle() {
  int fl = analogRead(FL_SENS);
  int fr = analogRead(FR_SENS);
  int lf = analogRead(LF_SENS);
  int rt = analogRead(RT_SENS);

  bool allWhite = isWhite(fl) && isWhite(fr) && isWhite(lf) && isWhite(rt);
  bool allBlack = isBlack(fl) && isBlack(fr) && isBlack(lf) && isBlack(rt);

 // return allWhite || allBlack;
 return false;
}

float getDistanceM() {
  long duration = getDistanceRaw();
  if (duration == 999999) return 5.0;
  return (duration * 0.000343) / 2.0;
}

// CHANGING STATE >> nextState = OTHER STATE!!; / REST HANDLED

// STATE DEF

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
  for (int i = 0; i < 20; i++) {
    turnLeft();
    delay(25);
    stopMotors();
    delay(50);

    if (candleDetected()) {
      Serial.println("Candle found: left");
      nextState = APPROACH;
      return;
    }
  }

  for (int i = 0; i < 40; i++) {
    turnRight();
    delay(25);
    stopMotors();
    delay(50);

    if (candleDetected()) {
      Serial.println("Candle found: right");
      nextState = APPROACH;
      return;
    }
  }

  for (int i = 0; i < 20; i++) {
    turnLeft();
    delay(25);
    stopMotors();
    delay(50);
  }

  Serial.println("Nothing found. Moving forward...");

  for (int i = 0; i < 10; i++) {
    forward();
    delay(250);
    stopMotors();
    delay(250);
  }
  stopMotors();
}

void findCandleExit() {
  stopMotors();
  Serial.println("Exiting FIND_CANDLE");
}

// APPROACH
unsigned long lastPulse = 0;

void approachEnter() {
  displayText("KYBEL > RUNNING", "STATE: APPROACH");
  Serial.println("Entering APPROACH");
  stopMotors();
}

void approachUpdate() {
  float d = getDistanceM();
  Serial.print("Approach dist: ");
  Serial.println(d);

  if (d < 0.025) {  
    Serial.println("wall detected");
    backward();
    delay(500); 
    stopMotors();
    nextState = FIND_FIRE;
    return;
  }
// inside
  if (insideCircle()) {
    Serial.println("circle detected changing state to FIND_FIRE");
    stopMotors();
    nextState = FIND_FIRE;
    return;
  }

  int pulseTime;
// too far
  if (d > 0.60) {
    pulseTime = 300;
  } 
  else if (d > 0.40) {
// med distance
    pulseTime = 200;
  } 
  else if (d > 0.20) {
// near 
    pulseTime = 120;
  } 
  else {
// very near
    pulseTime = 70;
  }

// pulsing movement

  if (d < 0.15) {
    Serial.println("Close to obstacle → stopping.");
    stopMotors();
    nextState = FIND_FIRE;
    return;
  }
// moving logic
  unsigned long now = millis();
  if (now - lastPulse * 2) {
    Serial.print("Pulse forward: ");
    Serial.println(pulseTime);

    forward();
    delay(pulseTime);
    stopMotors();
    lastPulse = now;
  }
}

void approachExit() {
  stopMotors();
  Serial.println("Exiting APPROACH");
}

// FIND_FIRE
void findFireEnter() {
  displayText("KYBEL > RUNNING", "STATE: FIND_FIRE");
}
void findFireUpdate() {
  nextState = KILL_FIRE;
}
void findFireExit() {}

// KILL_FIRE
void killFireEnter() {
  displayText("KYBEL > RUNNING", "STATE: KILL_FIRE");
  digitalWrite(VENT_RELAY, HIGH);
}
void killFireUpdate() {
  delay(2000);
  digitalWrite(VENT_RELAY, LOW);
}
void killFireExit() {
  
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
  nextState = FIND_CANDLE;
}
void calibrateExit() {}

// IDLE
void idleEnter() {
  nextState = CALIBRATE;
}
void idleUpdate() {
  //turnLeft();
}
void idleExit() {}

// ERR
void errEnter() {
  Serial.println("Entering ERR state");
}
void errUpdate() {
  digitalWrite(RESET_PIN, HIGH);
}
void errExit() {}

// krakotina

can you make it actually work ignore the circle function but its soo unprecise can you make it more precise and dont make it start the relay until the fire sensor one of any has the value above 500
