// Motor test: spins each motor FORWARD for 2 seconds, one at a time, and prints which one.
// Board: Arduino Uno. Lift the wheels, battery ON, Serial Monitor at 9600.
// Write down for each step: WHICH wheel spun (front-left, front-right, rear-left, rear-right)
// and WHICH WAY (forward = the way the car should drive, or backward).

#include <AFMotor.h>

AF_DCMotor m1(1, MOTOR12_1KHZ);
AF_DCMotor m2(2, MOTOR12_1KHZ);
AF_DCMotor m3(3, MOTOR34_1KHZ);
AF_DCMotor m4(4, MOTOR34_1KHZ);
AF_DCMotor* motors[] = { &m1, &m2, &m3, &m4 };

void setup() {
  Serial.begin(9600);
  Serial.println(F("Motor test starting in 3 s - lift the wheels!"));
  delay(3000);
  for (int i = 0; i < 4; i++) {
    Serial.print(F("Now spinning M")); Serial.print(i + 1); Serial.println(F(" FORWARD"));
    motors[i]->setSpeed(170);
    motors[i]->run(FORWARD);
    delay(2000);
    motors[i]->run(RELEASE);
    delay(1500);
  }
  Serial.println(F("Done. Press RESET to run again."));
}

void loop() {}
