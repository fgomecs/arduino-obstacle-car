// Obstacle-avoiding car v2
// Board: Arduino Uno R3 + Adafruit Motor Shield v1 (L293D), 4 DC motors, SG90 servo, HC-SR04
// Libraries: "Adafruit Motor Shield library" (AFMotor, v1), "NewPing", Servo (built-in)
//
// What changed vs v1:
//  - No more stutter: v1 ramped speed 0->150 on EVERY loop pass. Now it only ramps from a stop.
//  - "No echo" (0 cm) is treated as open road, not as a wall. v1 stopped and scanned in open spaces.
//  - Median-of-3 sensor reads to kill noise spikes.
//  - Reads the sensor continuously instead of driving blind for ~600 ms per loop.
//  - Slows down when something is near, backs up before turning, turns around when boxed in.
//  - Serial control (9600 baud) -> ready for a WiFi bridge later. Type in Serial Monitor:
//      a = auto mode   m = manual mode   f = forward   b = back   l = left   r = right   s = stop
//    Manual mode still stops on its own if you drive at a wall.

#include <AFMotor.h>
#include <Servo.h>
#include <NewPing.h>

// ---------- Pins / hardware ----------
#define TRIG_PIN      A0
#define ECHO_PIN      A1
#define SERVO_PIN     10      // SERVO_1 header on the shield
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
#define MANUAL_TURN_MS 350    // spin time per l/r command in manual mode

#define LOOK_RIGHT    36
#define LOOK_CENTER   90
#define LOOK_LEFT     144

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
AF_DCMotor leftMotor1(1, MOTOR12_1KHZ);
AF_DCMotor leftMotor2(2, MOTOR12_1KHZ);
AF_DCMotor rightMotor1(3, MOTOR34_1KHZ);
AF_DCMotor rightMotor2(4, MOTOR34_1KHZ);
Servo eyes;

enum Motion { STOPPED, FWD, BACK, SPIN };
Motion motion = STOPPED;
uint8_t currentSpeed = 0;
bool autoMode = true;
unsigned long lastReport = 0;

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
  currentSpeed = 0;
}

// Ramp up only when starting from a stop (protects batteries); otherwise just adjust speed.
void moveForward(uint8_t spd) {
  if (motion != FWD) {
    for (int s = 0; s <= spd; s += 5) { drive(FORWARD, s, FORWARD, s); delay(5); }
  }
  drive(FORWARD, spd, FORWARD, spd);
  motion = FWD;
  currentSpeed = spd;
}

void moveBackward(uint8_t spd) {
  if (motion != BACK) {
    for (int s = 0; s <= spd; s += 5) { drive(BACKWARD, s, BACKWARD, s); delay(5); }
  }
  drive(BACKWARD, spd, BACKWARD, spd);
  motion = BACK;
  currentSpeed = spd;
}

void spin(bool left, unsigned int ms) {
  if (left) drive(BACKWARD, CRUISE_SPEED + TURN_BOOST, FORWARD, CRUISE_SPEED);
  else      drive(FORWARD, CRUISE_SPEED, BACKWARD, CRUISE_SPEED + TURN_BOOST);
  motion = SPIN;
  delay(ms);
  moveStop();
}

// ---------- Sensor ----------
int readDistance() {
  unsigned int uS = sonar.ping_median(3);   // 3 pings, returns median echo time
  if (uS == 0) return MAX_DISTANCE;         // no echo = nothing in range = open road
  return uS / US_ROUNDTRIP_CM;
}

int lookAt(int angle, int settleMs) {
  eyes.write(angle);
  delay(settleMs);
  return readDistance();
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
  // Safety: even in manual, don't drive into a wall.
  if (motion == FWD && readDistance() < COLL_DIST) {
    moveStop();
    Serial.println(F("BLOCKED"));
  }
  delay(10);
}

void handleSerial() {
  while (Serial.available()) {
    char c = tolower(Serial.read());
    switch (c) {
      case 'a': autoMode = true;  Serial.println(F("MODE AUTO"));   break;
      case 'm': autoMode = false; moveStop(); Serial.println(F("MODE MANUAL")); break;
      case 's': moveStop(); autoMode = false; Serial.println(F("STOP")); break;
      case 'f': if (!autoMode) moveForward(CRUISE_SPEED);  break;
      case 'b': if (!autoMode) moveBackward(CRUISE_SPEED); break;
      case 'l': if (!autoMode) spin(true,  MANUAL_TURN_MS); break;
      case 'r': if (!autoMode) spin(false, MANUAL_TURN_MS); break;
      default: break;   // ignore newlines etc.
    }
  }
}

// ---------- Arduino entry points ----------
void setup() {
  Serial.begin(9600);
  eyes.attach(SERVO_PIN);
  eyes.write(LOOK_CENTER);
  moveStop();
  Serial.println(F("Car v2 ready. a=auto m=manual f/b/l/r/s"));
  delay(2000);   // time to set it on the floor
}

void loop() {
  handleSerial();
  if (autoMode) autoStep();
  else          manualStep();
}
