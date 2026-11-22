// Obstacle-avoiding car v3 (WiFi-ready)
// Board: Arduino Uno R3 + Adafruit Motor Shield v1 (L293D), 4 DC motors, SG90 servo, HC-SR04
// Libraries: "Adafruit Motor Shield library" (AFMotor, v1), "NewPing", Servo + SoftwareSerial (built-in)
//
// Same driving logic as v2, plus a second serial link to the NodeMCU ESP8266 bridge
// (esp8266_bridge/). The phone web page sends the same one-letter commands:
//      a = auto   m = manual   f = forward   b = back   l = left   r = right   s = stop
//
// Safety for phone driving: commands from the bridge must keep coming (the page repeats them
// every ~150 ms while you hold a button). If they stop for 600 ms (finger lifted, phone locked,
// WiFi dropped) the car stops by itself. Commands typed in the Serial Monitor are not timed out.
//
// Status line to the bridge every 300 ms:  S,<A|M>,<F|B|T|X>,<cm>
//   mode A=auto M=manual, motion F=fwd B=back T=turning X=stopped, distance in cm
//
// Wiring, no soldering: the servo is on the SERVO_2 header, the NodeMCU uses the free SER1 header.
//   SER1  S  -> NodeMCU D6        SER1  +  -> NodeMCU Vin        SER1  -  -> NodeMCU GND
// On power-up the sensor head wiggles left-right. If it does NOT move, your shield has the
// two headers' pins swapped: set SWAP_SERVO_PINS to 1 below and upload again.
// Optional return wire (distance/mode on the phone): A3 -> 1k -> NodeMCU D5, 2k from D5 to GND.

#include <AFMotor.h>
#include <Servo.h>
#include <NewPing.h>
#include <SoftwareSerial.h>

// ---------- Pins / hardware ----------
#define TRIG_PIN      A0
#define ECHO_PIN      A1
#define SWAP_SERVO_PINS 0     // 0 = standard labels (SERVO_2 = D9, SER1 = D10). 1 = swapped.
#if SWAP_SERVO_PINS
#define SERVO_PIN     10
#define BRIDGE_RX     9
#else
#define SERVO_PIN     9       // SERVO_2 header (servo plugged in here)
#define BRIDGE_RX     10      // SER1 header signal pin <- NodeMCU D6 (3.3 V is fine for the Uno)
#endif
#define BRIDGE_TX     A3      // optional -> NodeMCU D5 THROUGH a 1k/2k voltage divider
#define MAX_DISTANCE  300     // cm

// ---------- Tuning ----------
#define CRUISE_SPEED  150     // 0-255
#define SLOW_SPEED    100     // used when an object is getting close
#define TURN_BOOST    40      // extra power on the reversing side during a spin
#define COLL_DIST     30      // cm: stop and pick a new direction
#define SLOW_DIST     60      // cm: slow down
#define BACKUP_MS     300     // reverse this long before scanning
#define TURN_MS       1500    // spin time for a left/right turn (tune on your floor)
#define TURNAROUND_MS 1700    // spin time when boxed in
#define MANUAL_TURN_MS 350    // spin time per l/r typed in the Serial Monitor
#define BRIDGE_TIMEOUT_MS 600 // phone driving: stop if no command for this long
#define STATUS_MS     300     // status line to the bridge

#define LOOK_RIGHT    36
#define LOOK_CENTER   90
#define LOOK_LEFT     144

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
AF_DCMotor leftMotor1(1, MOTOR12_1KHZ);
AF_DCMotor leftMotor2(2, MOTOR12_1KHZ);
AF_DCMotor rightMotor1(3, MOTOR34_1KHZ);
AF_DCMotor rightMotor2(4, MOTOR34_1KHZ);
Servo eyes;
SoftwareSerial bridge(BRIDGE_RX, BRIDGE_TX);

enum Motion { STOPPED, FWD, BACK, SPIN };
Motion motion = STOPPED;
bool autoMode = true;            // set false for bench testing (waits for commands)
bool bridgeDriving = false;      // last manual move came from the phone
unsigned long lastBridgeCmd = 0;
unsigned long lastReport = 0;
unsigned long lastStatus = 0;
int lastDist = MAX_DISTANCE;

// ---------- Low-level motor helpers ----------
void drive(uint8_t lDir, uint8_t lSpd, uint8_t rDir, uint8_t rSpd) {
  leftMotor1.run(lDir);  leftMotor2.run(lDir);
  rightMotor1.run(rDir); rightMotor2.run(rDir);
  leftMotor1.setSpeed(lSpd);  leftMotor2.setSpeed(lSpd);
  rightMotor1.setSpeed(rSpd); rightMotor2.setSpeed(rSpd);
}

void moveStop() {
  drive(RELEASE, 0, RELEASE, 0);
  motion = STOPPED;
}

// Ramp up only when starting from a stop (protects batteries); otherwise just adjust speed.
void moveForward(uint8_t spd) {
  if (motion != FWD) {
    for (int s = 0; s <= spd; s += 5) { drive(FORWARD, s, FORWARD, s); delay(5); }
  }
  drive(FORWARD, spd, FORWARD, spd);
  motion = FWD;
}

void moveBackward(uint8_t spd) {
  if (motion != BACK) {
    for (int s = 0; s <= spd; s += 5) { drive(BACKWARD, s, BACKWARD, s); delay(5); }
  }
  drive(BACKWARD, spd, BACKWARD, spd);
  motion = BACK;
}

void spinStart(bool left) {      // keeps spinning until stopped (used for phone driving)
  if (left) drive(BACKWARD, CRUISE_SPEED + TURN_BOOST, FORWARD, CRUISE_SPEED);
  else      drive(FORWARD, CRUISE_SPEED, BACKWARD, CRUISE_SPEED + TURN_BOOST);
  motion = SPIN;
}

void spin(bool left, unsigned int ms) {   // timed spin
  spinStart(left);
  delay(ms);
  moveStop();
}

// ---------- Sensor ----------
int readDistance() {
  unsigned int uS = sonar.ping_median(3);   // 3 pings, returns median echo time
  int d = (uS == 0) ? MAX_DISTANCE : uS / US_ROUNDTRIP_CM;   // no echo = open road
  lastDist = d;
  return d;
}

int lookAt(int angle, int settleMs) {
  eyes.write(angle);
  delay(settleMs);
  return readDistance();
}

// ---------- Status to the bridge ----------
void sendStatus() {
  char m = 'X';
  if (motion == FWD) m = 'F'; else if (motion == BACK) m = 'B'; else if (motion == SPIN) m = 'T';
  bridge.print(F("S,"));
  bridge.print(autoMode ? 'A' : 'M');
  bridge.print(',');
  bridge.print(m);
  bridge.print(',');
  bridge.println(lastDist);
}

// ---------- Behaviors ----------
void avoidObstacle() {
  moveStop();
  delay(100);
  moveBackward(CRUISE_SPEED);
  delay(BACKUP_MS);
  moveStop();

  int rightDist = lookAt(LOOK_RIGHT, 400);
  int leftDist  = lookAt(LOOK_LEFT, 600);   // longer sweep (right -> left)
  eyes.write(LOOK_CENTER);
  delay(200);

  Serial.print(F("SCAN L=")); Serial.print(leftDist);
  Serial.print(F(" R=")); Serial.println(rightDist);

  if (leftDist < COLL_DIST && rightDist < COLL_DIST) spin(false, TURNAROUND_MS); // boxed in
  else if (leftDist >= rightDist)                     spin(true, TURN_MS);
  else                                                spin(false, TURN_MS);
}

void autoStep() {
  int d = readDistance();

  if (millis() - lastReport > 500) {          // status line twice a second
    Serial.print(F("DIST ")); Serial.println(d);
    lastReport = millis();
  }

  if (d < COLL_DIST)      avoidObstacle();
  else if (d < SLOW_DIST) moveForward(SLOW_SPEED);
  else                    moveForward(CRUISE_SPEED);
}

void manualStep() {
  // Phone driving watchdog: no fresh command -> stop.
  if (bridgeDriving && motion != STOPPED && millis() - lastBridgeCmd > BRIDGE_TIMEOUT_MS) {
    moveStop();
    bridgeDriving = false;
  }
  // Safety: even in manual, don't drive into a wall. (Also keeps lastDist fresh for the phone.)
  if (motion == FWD || millis() - lastStatus > STATUS_MS) {
    if (readDistance() < COLL_DIST && motion == FWD) {
      moveStop();
      Serial.println(F("BLOCKED"));
    }
  }
  delay(10);
}

void handleCommand(char c, bool fromBridge) {
  c = tolower(c);
  if (fromBridge && strchr("fblr", c)) {     // phone drive keys repeat while held
    lastBridgeCmd = millis();
    bridgeDriving = true;
  }
  switch (c) {
    case 'a': autoMode = true;  Serial.println(F("MODE AUTO"));   break;
    case 'm': autoMode = false; moveStop(); Serial.println(F("MODE MANUAL")); break;
    case 's': moveStop(); autoMode = false; bridgeDriving = false; Serial.println(F("STOP")); break;
    case 'f':
      if (!autoMode && readDistance() >= COLL_DIST) moveForward(CRUISE_SPEED);
      else if (!autoMode) Serial.println(F("BLOCKED"));
      break;
    case 'b': if (!autoMode) moveBackward(CRUISE_SPEED); break;
    case 'l':
      if (!autoMode) { if (fromBridge) { if (motion != SPIN) spinStart(true); } else spin(true, MANUAL_TURN_MS); }
      break;
    case 'r':
      if (!autoMode) { if (fromBridge) { if (motion != SPIN) spinStart(false); } else spin(false, MANUAL_TURN_MS); }
      break;
    default: break;   // ignore newlines etc.
  }
}

void handleInputs() {
  while (Serial.available()) handleCommand(Serial.read(), false);
  while (bridge.available()) handleCommand(bridge.read(), true);
}

// ---------- Arduino entry points ----------
void setup() {
  Serial.begin(9600);
  bridge.begin(9600);
  eyes.attach(SERVO_PIN);
  moveStop();
  // Boot wiggle: proves the servo pin is right (no movement -> set SWAP_SERVO_PINS 1)
  eyes.write(60);  delay(350);
  eyes.write(120); delay(450);
  eyes.write(LOOK_CENTER);
  Serial.print(F("Servo on D")); Serial.print(SERVO_PIN);
  Serial.print(F(", WiFi bridge on D")); Serial.println(BRIDGE_RX);
  Serial.println(F("Car v3 ready. a=auto m=manual f/b/l/r/s (USB or WiFi bridge)"));
  delay(2000);   // time to set it on the floor
}

void loop() {
  handleInputs();
  if (autoMode) autoStep();
  else          manualStep();
  if (millis() - lastStatus > STATUS_MS) {
    sendStatus();
    lastStatus = millis();
  }
}
