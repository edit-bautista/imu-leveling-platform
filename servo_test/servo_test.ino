// servo_test.ino — Step 1: command the SG90 to an angle without resetting the Mega.
// Save as servo_test/servo_test.ino (Arduino needs folder name = file name).
//
// Wiring (SG90 has 3 wires):
//   Brown  -> GND
//   Red    -> 5V   (Mega 5V first; if the board resets, use the breadboard
//                   power module's 5V instead AND tie its GND to Mega GND)
//   Orange -> pin 9 (signal)
//
// Usage: open Serial Monitor at 115200, set line ending to "Newline",
// type an angle (0-180) and press Enter. The servo moves there.

#include <Servo.h>

const int SERVO_PIN = 9;
Servo platformServo;

void setup() {
  Serial.begin(115200);
  platformServo.attach(SERVO_PIN);   // starts the 50 Hz pulse train on pin 9
  platformServo.write(90);           // start at the middle of the range
  Serial.println("Servo at 90. Type an angle 0-180 and press Enter.");
  Serial.print("Boot count marker, t_ms = ");
  Serial.println(millis());          // if you see this line again, the board reset
}

void loop() {
  if (Serial.available()) {
    int angle = Serial.parseInt();   // reads digits until the newline
    while (Serial.available()) Serial.read();  // throw away the leftover newline

    if (angle >= 0 && angle <= 180) {
      platformServo.write(angle);
      Serial.print("Moved to ");
      Serial.println(angle);
    } else {
      Serial.println("Out of range, use 0-180.");
    }
  }
}
