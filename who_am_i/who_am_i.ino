// who_am_i.ino — Step 1: prove the Mega can talk to the IMU (MPU-6500) over I2C.
// Wiring (Mega 2560): VCC->5V, GND->GND, SDA->pin 20, SCL->pin 21
// Expected Serial Monitor output (115200 baud):
//   Wake write result: 0  (0 = OK)
//   WHO_AM_I = 0x68 (MPU-6050), 0x70 (MPU-6500), 0x71 (MPU-9250) or 0x73 (MPU-9255)

#include <Wire.h>

const uint8_t MPU_ADDR   = 0x68;  // 7-bit I2C address (AD0 pin low)
const uint8_t PWR_MGMT_1 = 0x6B;  // power management register
const uint8_t WHO_AM_I   = 0x75;  // identity register: 0x68/0x70/0x71/0x73 depending on chip

void setup() {
  Serial.begin(115200);
  while (!Serial) {}              // wait for Serial Monitor (harmless on Mega)
  Wire.begin();                   // join the I2C bus as controller

  // 1) Wake the chip: it powers up in sleep mode. Write 0x00 to PWR_MGMT_1.
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(PWR_MGMT_1);         // which register
  Wire.write(0x00);               // value: clear the SLEEP bit
  uint8_t result = Wire.endTransmission();  // actually sends; returns status
  Serial.print("Wake write result: ");
  Serial.print(result);
  Serial.println("  (0 = OK, 2 = no device answered -> check wiring)");

  // 2) Read WHO_AM_I: tell the chip which register, then ask for 1 byte.
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(WHO_AM_I);
  Wire.endTransmission(false);    // false = repeated start, keep the bus
  Wire.requestFrom(MPU_ADDR, (uint8_t)1);

  if (Wire.available()) {
    uint8_t id = Wire.read();
    Serial.print("WHO_AM_I = 0x");
    Serial.println(id, HEX);
    // The ID tells us which chip is really on the board:
    if      (id == 0x68) Serial.println("MPU-6050 found. I2C works.");
    else if (id == 0x70) Serial.println("MPU-6500 found. I2C works.");
    else if (id == 0x71) Serial.println("MPU-9250 found. I2C works.");
    else if (id == 0x73) Serial.println("MPU-9255 found. I2C works.");
    else                 Serial.println("Unknown ID — tell Claude this value.");
  } else {
    Serial.println("No byte returned — check SDA/SCL pins.");
  }
}

void loop() {
  // Nothing yet. Next session: burst-read 14 bytes from 0x3B here.
}
