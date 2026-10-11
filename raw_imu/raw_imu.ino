// raw_imu.ino — Session 2: read all 6 axes from the IMU and print CSV lines
// Board: Arduino Mega 2560 (SDA = pin 20, SCL = pin 21)
// Chip:  MPU-6500 (WHO_AM_I = 0x70), same register map as the MPU-6050

#include <Wire.h>

const uint8_t IMU_ADDR     = 0x68;  // I2C address of the IMU (AD0 pin low)
const uint8_t PWR_MGMT_1   = 0x6B;  // power register: write 0x00 to wake the chip
const uint8_t ACCEL_XOUT_H = 0x3B;  // first of the 14 data registers

// Scale factors at the chip's default ranges
const float ACCEL_SCALE = 16384.0;  // counts per g      (range +/-2 g)
const float GYRO_SCALE  = 131.0;    // counts per deg/s  (range +/-250 deg/s)

// Read the next 2 bytes from the I2C buffer and combine them into one signed number.
// The chip sends the HIGH byte first, then the LOW byte.
int16_t read16() {
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);  // int16_t = signed 16-bit, so two's complement gives negatives
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);  // 400 kHz "fast mode" I2C, helps us reach 100 Hz later

  // Wake the IMU (it powers up asleep)
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);

  Serial.println("t_ms,ax,ay,az,gx,gy,gz");  // CSV header
}

void loop() {
  // 1. Point the chip at register 0x3B
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);  // false = keep the bus (repeated start) so the read follows right away

  // 2. Burst-read 14 bytes. The chip auto-advances to the next register each byte:
  //    accel X,Y,Z (6 bytes) | temperature (2 bytes) | gyro X,Y,Z (6 bytes)
  Wire.requestFrom(IMU_ADDR, (uint8_t)14);
  if (Wire.available() < 14) return;  // incomplete read: skip this loop

  int16_t rawAx = read16();
  int16_t rawAy = read16();
  int16_t rawAz = read16();
  read16();                           // temperature: read it to move past it, then ignore
  int16_t rawGx = read16();
  int16_t rawGy = read16();
  int16_t rawGz = read16();

  // 3. Convert raw counts to real units
  float ax = rawAx / ACCEL_SCALE;  // g
  float ay = rawAy / ACCEL_SCALE;
  float az = rawAz / ACCEL_SCALE;
  float gx = rawGx / GYRO_SCALE;   // deg/s
  float gy = rawGy / GYRO_SCALE;
  float gz = rawGz / GYRO_SCALE;

  // 4. Print one CSV line: t_ms,ax,ay,az,gx,gy,gz
  Serial.print(millis());  Serial.print(',');
  Serial.print(ax, 3);     Serial.print(',');
  Serial.print(ay, 3);     Serial.print(',');
  Serial.print(az, 3);     Serial.print(',');
  Serial.print(gx, 2);     Serial.print(',');
  Serial.print(gy, 2);     Serial.print(',');
  Serial.println(gz, 2);

  delay(10);  // roughly 100 lines per second for now; we measure the real rate later
}
