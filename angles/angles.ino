// angles.ino — Step 2: gyro bias calibration + three tilt angles.
// Save as angles/angles.ino (Arduino needs folder name = file name).
//
// Wiring (Mega 2560): VCC->5V, GND->GND, SDA->pin 20, SCL->pin 21
// Lay the board FLAT, chip facing UP, and keep it still for the first ~3 seconds.
//
// Output, 115200 baud, CSV:  t_ms,gx,accel_angle,gyro_angle,filt_angle
//   gx          = roll rate in deg/s after bias removal (should be ~0 when still)
//   accel_angle = tilt from gravity alone (accurate long-term, jumpy)
//   gyro_angle  = tilt from adding up rotation rate (smooth, but drifts)
//   filt_angle  = complementary filter: the best of both

#include <Wire.h>

const uint8_t MPU_ADDR     = 0x68;
const uint8_t PWR_MGMT_1   = 0x6B;
const uint8_t ACCEL_XOUT_H = 0x3B;

const float ACCEL_SCALE = 16384.0;  // raw counts per g   (default +/-2 g range)
const float GYRO_SCALE  = 131.0;    // raw counts per deg/s (default +/-250 deg/s range)
const float ALPHA       = 0.98;     // filter weight on the gyro
const int   CAL_SAMPLES = 500;      // samples averaged for gyro bias

float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;  // deg/s, measured at startup
float gyroAngle = 0;     // gyro-only angle (deg)
float filtAngle = 0;     // filtered angle (deg)
unsigned long lastMicros = 0;

// Read two bytes (high first) and combine them into a signed 16-bit number.
// Two separate reads on purpose: C++ doesn't guarantee which side of
// (Wire.read() << 8) | Wire.read() runs first.
int16_t read16() {
  uint8_t high = Wire.read();
  uint8_t low  = Wire.read();
  return (int16_t)((high << 8) | low);   // int16_t = two's complement, so negatives work
}

// Burst-read all 14 data bytes starting at 0x3B and scale them.
void readIMU(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);            // repeated start: keep the bus
  Wire.requestFrom(MPU_ADDR, (uint8_t)14);

  ax = read16() / ACCEL_SCALE;
  ay = read16() / ACCEL_SCALE;
  az = read16() / ACCEL_SCALE;
  read16();                               // temperature: read and ignore
  gx = read16() / GYRO_SCALE;
  gy = read16() / GYRO_SCALE;
  gz = read16() / GYRO_SCALE;
}

// Tilt from gravity: how much of gravity lands on y vs z. Result in degrees.
float accelAngle(float ay, float az) {
  return atan2(ay, az) * 180.0 / PI;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);                  // I2C fast mode (400 kHz) so reads take less time

  // Wake the chip (it starts asleep).
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);

  // Gyro bias calibration: board still, so the true rotation rate is 0.
  // Whatever the gyro reads on average is its bias. Subtract it forever after.
  Serial.println("Calibrating gyro, keep the board still...");
  float ax, ay, az, gx, gy, gz;
  float sumX = 0, sumY = 0, sumZ = 0;
  for (int i = 0; i < CAL_SAMPLES; i++) {
    readIMU(ax, ay, az, gx, gy, gz);
    sumX += gx;
    sumY += gy;
    sumZ += gz;
    delay(3);
  }
  gyroBiasX = sumX / CAL_SAMPLES;
  gyroBiasY = sumY / CAL_SAMPLES;
  gyroBiasZ = sumZ / CAL_SAMPLES;
  Serial.print("Gyro bias (deg/s): ");
  Serial.print(gyroBiasX); Serial.print(", ");
  Serial.print(gyroBiasY); Serial.print(", ");
  Serial.println(gyroBiasZ);

  // Start both integrated angles at the accelerometer's answer, not at 0.
  readIMU(ax, ay, az, gx, gy, gz);
  gyroAngle = accelAngle(ay, az);
  filtAngle = gyroAngle;

  Serial.println("t_ms,gx,accel_angle,gyro_angle,filt_angle");
  lastMicros = micros();
}

void loop() {
  float ax, ay, az, gx, gy, gz;
  readIMU(ax, ay, az, gx, gy, gz);
  gx -= gyroBiasX;                        // remove the bias measured at startup

  // dt = time since last loop, in seconds. micros() counts microseconds.
  unsigned long now = micros();
  float dt = (now - lastMicros) / 1000000.0;
  lastMicros = now;

  float acc = accelAngle(ay, az);

  // Gyro only: angle = angle + rate * time. Any tiny error gets added every loop -> drift.
  gyroAngle += gx * dt;

  // Complementary filter: 98% "previous angle + gyro change", 2% accelerometer.
  // The gyro handles fast motion; the accelerometer slowly pulls drift back to truth.
  filtAngle = ALPHA * (filtAngle + gx * dt) + (1.0 - ALPHA) * acc;

  Serial.print(millis());   Serial.print(',');
  Serial.print(gx, 2);      Serial.print(',');
  Serial.print(acc, 2);     Serial.print(',');
  Serial.print(gyroAngle, 2); Serial.print(',');
  Serial.println(filtAngle, 2);
}
