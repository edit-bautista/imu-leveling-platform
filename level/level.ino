// level.ino — Closed-loop leveling (single axis)
// Save as level/level.ino inside imu-leveling-platform (folder name = file name).
//
// Board: Arduino Mega 2560. IMU: GY-521 board (MPU-6500 chip) on SDA pin 20 / SCL pin 21.
// Servo: SG90, signal on pin 9 (PWM row), red -> 5V, brown -> GND.
// Mount: card face toward you, Y arrow UP, servo shaft along the chip's Z axis.
//   -> tilt angle comes from ax and ay, rotation rate comes from gz.
//
// HOW IT WORKS: every 5 ms (200 Hz), measure the card's tilt with the
// complementary filter, then nudge the servo command a little in the opposite
// direction. The nudge is proportional to the error, so it keeps adjusting
// until the card reads ~0 degrees. (Integral control: the servo command is the
// running sum of past corrections, so it doesn't matter where "level" sits on
// the servo. The IMU decides.)
//
// STEP 1: SERVO_ENABLED = false -> sensing only. Tilt the box. filt_deg should
//         follow accel_deg smoothly. If filt_deg lurches the WRONG way first and
//         then creeps back, set GYRO_SIGN = -1.0.
// STEP 2: SERVO_ENABLED = true -> closed loop. If the card runs AWAY from level
//         and slams into a limit, set DIRECTION = -1.0.
//
// Serial output (115200 baud), one CSV line per loop:
//   t_ms,accel_deg,filt_deg,servo_cmd

#include <Wire.h>
#include <Servo.h>

// ---------- settings you may change ----------
const bool  SERVO_ENABLED = true;  // false = sensing only, true = closed loop
const float GYRO_SIGN     = 1.0;    // flip to -1.0 if the gyro fights the accelerometer
const float DIRECTION     = 1.0;    // flip to -1.0 if the servo runs away from level
const float ANGLE_OFFSET  = 0.0;    // degrees; set to accel_deg's reading when the card is truly level
const float KI            = 3.0;    // servo degrees per second, per degree of tilt error
const float DEADBAND_DEG  = 0.5;    // ignore errors smaller than this (stops jitter from gear slop)
const float ALPHA         = 0.98;   // complementary filter: 98% gyro, 2% accelerometer
const unsigned long LOOP_US = 5000; // 5000 microseconds = 200 Hz control loop

// ---------- IMU registers ----------
const uint8_t IMU_ADDR     = 0x68;  // I2C address (AD0 pin low)
const uint8_t PWR_MGMT_1   = 0x6B;  // write 0x00 to wake the chip
const uint8_t WHO_AM_I     = 0x75;  // identity register
const uint8_t ACCEL_XOUT_H = 0x3B;  // first of 14 data registers
const float ACCEL_SCALE = 16384.0;  // counts per g at +/-2 g
const float GYRO_SCALE  = 131.0;    // counts per deg/s at +/-250 deg/s

const int SERVO_PIN = 9;
Servo platformServo;

float gzBias   = 0.0;   // gyro offset measured at startup
float angle    = 0.0;   // filtered tilt angle, degrees
float servoCmd = 90.0;  // servo command, degrees (start mid-range)
unsigned long lastUs = 0;

// Combine high byte + low byte into one signed 16-bit number (two's complement).
int16_t read16() {
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

// Burst-read 14 bytes from 0x3B. Returns false if the read came back short.
bool readImu(float &ax, float &ay, float &gz) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);           // repeated start: keep the bus
  Wire.requestFrom(IMU_ADDR, (uint8_t)14);
  if (Wire.available() < 14) return false;

  int16_t rawAx = read16();
  int16_t rawAy = read16();
  read16();                              // az (not needed for this axis)
  read16();                              // temperature (skip)
  read16();                              // gx (skip)
  read16();                              // gy (skip)
  int16_t rawGz = read16();

  ax = rawAx / ACCEL_SCALE;              // g
  ay = rawAy / ACCEL_SCALE;              // g
  gz = rawGz / GYRO_SCALE;               // deg/s
  return true;
}

// Tilt from gravity: 0 deg when Y points straight up.
float accelAngle(float ax, float ay) {
  return atan2(ay, -ax) * 180.0 / PI - ANGLE_OFFSET;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);                 // 400 kHz fast-mode I2C

  // Wake the IMU
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);

  // Identity check (0x70 = MPU-6500, 0x68 = MPU-6050)
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(WHO_AM_I);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, (uint8_t)1);
  Serial.print("# WHO_AM_I = 0x");
  Serial.println(Wire.read(), HEX);

  if (SERVO_ENABLED) {
    platformServo.attach(SERVO_PIN);
    platformServo.write((int)servoCmd);
    delay(1500);   // let the servo finish moving and the card stop swinging before calibrating
  }

  // Gyro bias calibration: hold still while we average 500 readings
  Serial.println("# Calibrating gyro, hands off...");
  float sum = 0, ax, ay, gz;
  int good = 0;
  for (int i = 0; i < 500; i++) {
    if (readImu(ax, ay, gz)) { sum += gz; good++; }
    delay(3);
  }
  gzBias = (good > 0) ? sum / good : 0.0;
  Serial.print("# gz bias (deg/s) = ");
  Serial.println(gzBias, 3);

  // Start the filter at the accelerometer's angle so it doesn't ramp from 0
  if (readImu(ax, ay, gz)) angle = accelAngle(ax, ay);

  Serial.println("t_ms,accel_deg,filt_deg,servo_cmd");
  lastUs = micros();
}

void loop() {
  // Fixed-rate loop: wait until 5 ms have passed since the last loop
  unsigned long now = micros();
  if (now - lastUs < LOOP_US) return;
  float dt = (now - lastUs) / 1000000.0;  // seconds
  lastUs = now;

  float ax, ay, gz;
  if (!readImu(ax, ay, gz)) return;

  // 1. Sense: complementary filter
  float rate     = GYRO_SIGN * (gz - gzBias);          // deg/s, bias removed
  float accelDeg = accelAngle(ax, ay);
  angle = ALPHA * (angle + rate * dt) + (1.0 - ALPHA) * accelDeg;

  // 2. Act: nudge the servo opposite the tilt (integral control)
  if (SERVO_ENABLED) {
    float error = angle;                               // target is 0 deg
    if (abs(error) > DEADBAND_DEG) {
      servoCmd -= DIRECTION * KI * error * dt;
      servoCmd = constrain(servoCmd, 0.0, 180.0);
    }
    platformServo.write((int)servoCmd);
  }

  // 3. Log
  Serial.print(millis());       Serial.print(',');
  Serial.print(accelDeg, 2);    Serial.print(',');
  Serial.print(angle, 2);       Serial.print(',');
  Serial.println(servoCmd, 1);
}
