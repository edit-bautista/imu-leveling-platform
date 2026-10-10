# Self-Leveling IMU Platform — Spec

## Goal
A single-axis platform that stays level when its base tilts. An MPU-6500 IMU (Inertial Measurement Unit) is read over I2C, a complementary filter fuses accelerometer + gyro into a stable roll angle, and an SG90 servo corrects the tilt. Data streams over UART to Python telemetry and later into ROS 2 as `sensor_msgs/Imu` on `/imu/data`.

## Users / workflows
- **Me (developer):** flash firmware → watch live plot → log CSV → measure drift, leveling error, loop rate.
- **Reviewer (lab / recruiter):** read README, watch 30-s demo, inspect code and measured results.
- **ROS 2 consumer:** subscribe to `/imu/data`, visualize in RViz, replay with rosbag.

## Hardware
- Arduino Mega 2560 (I2C: SDA = pin 20, SCL = pin 21 — not A4/A5)
- MPU-6500 breakout (board silkscreen "MPU-9250/6500/9255", chip marked MP65), I2C address 0x68. WHO_AM_I = 0x70 confirmed. Kit chart listed a GY-521 (MPU-6050); register map is the same for this project.
- SG90 servo (PWM)
- Breadboard power module if the servo browns out the Mega (shared GND)
- Host: MacBook Pro (Apple Silicon) → Parallels Ubuntu 24.04 ARM64 + ROS 2 Jazzy

## Design decisions
- Direct register access with `Wire` (no MPU library), so every byte is understood:
  - Wake: write `0x00` to `PWR_MGMT_1` (0x6B)
  - Identity check: `WHO_AM_I` (0x75) returns 0x70 (MPU-6500; an MPU-6050 would return 0x68)
  - Data: burst-read 14 bytes from `ACCEL_XOUT_H` (0x3B) — accel XYZ, temp, gyro XYZ (16-bit two's complement, big-endian)
- Scaling (default ranges): accel / 16384 = g (±2 g), gyro / 131 = °/s (±250 °/s)
- Gyro bias calibrated at startup (board still, average N samples)
- Tilt: `roll = atan2(ay, az)`
- Filter: `angle = 0.98 * (angle + gx * dt) + 0.02 * accel_angle`
- Serial 115200 baud, CSV lines: `t_ms,ax,ay,az,gx,gy,gz` (angles appended later)

## Acceptance criteria
- [ ] Holds platform level within **±3°** for base tilts up to **30°**
- [ ] Filtered angle drift **< 1° over 60 s** with board still
- [ ] Control loop runs at **≥ 100 Hz**
- [ ] ROS 2 node publishes `sensor_msgs/Imu` on `/imu/data`, viewable in RViz, recorded with rosbag

## Metrics to record
| Metric | Gyro-only | Filtered |
|---|---|---|
| 60-s drift (°) | | |
| Leveling error (± ° at max tilt) | — | |
| Loop rate (Hz) | — | |

## Non-goals
- No PID tuning rabbit hole (P or simple PD is enough to meet spec)
- No 3D printing
- No second axis

## Milestones
1. Repo + spec + WHO_AM_I read (0x70, MPU-6500) ✅
2. Raw 6-axis data + live Python plot
3. Tilt angles + complementary filter + first drift measurement
4. Servo mount + sweep test
5. Closed-loop leveling
6. Measurements + README + demo video
7. ROS 2 node on `/imu/data` → RViz → rosbag
