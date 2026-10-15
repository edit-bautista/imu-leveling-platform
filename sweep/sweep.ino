// sweep.ino — Step 2: step the servo through angles and record the tilt the IMU measures.
// Save as sweep/sweep.ino (replace the earlier version).
//
// Mounting (card taped flat against the servo arm, face toward you):
//   GY-521 taped flat on the card, chip facing YOU, Y arrow pointing UP.
//   That makes the chip's Z axis point along the servo shaft, so the servo
//   rotates the chip around Z. Tilt is measured from X and Y:
//     accel angle = atan2(ax, ay)   (0 deg when Y points straight up)
//     gyro rate   = gz              (spin around the shaft)
// Wiring: IMU on SDA 20 / SCL 21, servo signal on pin 9.
//
// Output (Serial Monitor, 115200): one CSV line per servo position:
//   direction,servo_cmd,measured_deg
// Keep everything still while it calibrates (first ~2 seconds).

#include <Wire.h>
#include <Servo.h>

// ---- IMU registers and scales (same as angles.ino) ----
const uint8_t IMU_ADDR     = 0x68;
const uint8_t PWR_MGMT_1   = 0x6B;
const uint8_t ACCEL_XOUT_H = 0x3B;
const float ACCEL_SCALE = 16384.0;   // counts per g
const float GYRO_SCALE  = 131.0;     // counts per deg/s
const float ALPHA = 0.98;            // complementary filter: 98% gyro, 2% accel

// ---- Sweep settings ----
const int SERVO_PIN   = 9;
const int START_ANGLE = 45;          // stay away from 0/180 so the card can't hit anything
const int END_ANGLE   = 135;
const int STEP        = 5;
const unsigned long SETTLE_MS = 700; // wait for the servo to arrive and stop wobbling
const unsigned long AVG_MS    = 300; // then average the angle over this window

Servo platformServo;
float gyroBiasZ = 0;                 // deg/s the gyro reads around Z while perfectly still
float angle = 0;                     // filtered tilt angle, degrees
unsigned long lastMicros = 0;

// Combine high byte + low byte into one signed 16-bit number (two's complement).
int16_t read16() {
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

// Burst-read 14 bytes from 0x3B; keep the ones we need for rotation around Z.
// Order on the chip: ax, ay, az, temp, gx, gy, gz (2 bytes each).
void readIMU(float &ax, float &ay, float &gz) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, (uint8_t)14);
  ax = read16() / ACCEL_SCALE;
  ay = read16() / ACCEL_SCALE;
  read16();                          // az (not needed: points along the shaft)
  read16();                          // temperature (skip)
  read16();                          // gx
  read16();                          // gy
  gz = read16() / GYRO_SCALE;
}

// One pass of the complementary filter.
void updateAngle() {
  float ax, ay, gz;
  readIMU(ax, ay, gz);
  unsigned long now = micros();
  float dt = (now - lastMicros) / 1000000.0;   // seconds since last update
  lastMicros = now;
  float accelAngle = atan2(ax, ay) * 180.0 / PI;
  angle = ALPHA * (angle + (gz - gyroBiasZ) * dt) + (1.0 - ALPHA) * accelAngle;
}

// Move the servo, let it settle, then return the average measured angle.
float holdAndMeasure(int cmd) {
  platformServo.write(cmd);
  unsigned long t0 = millis();
  while (millis() - t0 < SETTLE_MS) updateAngle();   // keep the filter running

  float sum = 0;
  long count = 0;
  t0 = millis();
  while (millis() - t0 < AVG_MS) {
    updateAngle();
    sum += angle;
    count++;
  }
  return sum / count;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  // Wake the IMU
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();

  platformServo.attach(SERVO_PIN);
  platformServo.write(90);
  delay(1000);                       // let the card stop moving before calibrating

  // Gyro bias calibration: average 500 still readings of gz
  float sum = 0;
  for (int i = 0; i < 500; i++) {
    float ax, ay, gz;
    readIMU(ax, ay, gz);
    sum += gz;
    delay(2);
  }
  gyroBiasZ = sum / 500.0;

  // Start the filter at the accelerometer's angle instead of 0
  float ax, ay, gz;
  readIMU(ax, ay, gz);
  angle = atan2(ax, ay) * 180.0 / PI;
  lastMicros = micros();

  Serial.print("# gyro bias z (deg/s): ");
  Serial.println(gyroBiasZ);
  Serial.println("direction,servo_cmd,measured_deg");

  // Sweep up, then back down (comparing the two shows backlash in the gears)
  for (int cmd = START_ANGLE; cmd <= END_ANGLE; cmd += STEP) {
    float m = holdAndMeasure(cmd);
    Serial.print("up,"); Serial.print(cmd); Serial.print(","); Serial.println(m, 2);
  }
  for (int cmd = END_ANGLE; cmd >= START_ANGLE; cmd -= STEP) {
    float m = holdAndMeasure(cmd);
    Serial.print("down,"); Serial.print(cmd); Serial.print(","); Serial.println(m, 2);
  }

  platformServo.write(90);
  Serial.println("# DONE");
}

void loop() {
  // Nothing: the sweep runs once. Press the Mega's RESET button to run it again.
}
