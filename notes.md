# Measurement notes

## Tue Oct 13, 2026 — first drift measurement (angles.ino)

Board flat, chip up, untouched. Gyro bias calibrated at startup (500 samples).
Log: `log_20261013_202504.csv` · computed with `telemetry/drift.py`

| Metric | Result | Spec |
|---|---|---|
| Filtered drift, 60 s | +0.04° | < 1° ✅ |
| Gyro-only drift, 60 s | −0.26° | — |
| Loop rate (sensing only, no servo) | 355 Hz | ≥ 100 Hz ✅ |

Gyro bias at startup (deg/s): −0.56, 3.22, 0.43 (x, y, z). Matches Sat Oct 10.

## Surprises (for README)
- Earlier run reached −21.6° gyro-only error after ~2 min of tilting while the filtered
  angle stayed at 0.3°. A bumped calibration left gx at about −1.9 °/s, and integration
  added that error hundreds of times per second.
- Loop rate jumped from ~77 Hz to ~355 Hz: removed `delay()`, shorter CSV lines,
  I2C fast mode (400 kHz).
- First plot showed nothing: `ser.in_waiting` never reported data on macOS. Reading
  with `readline()` and a short timeout fixed it.
- Arduino IDE checkmark = Verify (compile only). The arrow = Upload.

## To re-measure Friday
- Loop rate with the servo running (the number for the resume).
- Optional: gyro drift with bias calibration turned off, to show what calibration buys.

## Session: Closed-loop leveling (Thu Oct 22, 2026)

### Measurements (level/level.ino, logs/level_tilt*.csv)
Integral control, KI = 3.0, deadband 0.5°, complementary filter 0.98/0.02.
Leveling error = worst |filt_deg| over the last 10 s of a 15-s run.

| Box tilt | Leveling error (worst) | Mean angle | Servo cmd |
|---|---|---|---|
| 0°   | ±0.18° | -0.15° | 88.7  |
| ~10° | ±2.62° | +0.12° | 97.0  |
| ~20° | ±0.18° | -0.14° | 108.8 |
| ~30° | ±0.46° | +0.43° | 118.3 |

- Loop rate: 200 Hz with the servo running (fixed 5 ms loop)
- Servo moved ~29.6° for the ~30° tilt, so it is undoing the tilt about 1:1
- Spec check: ±3° at 30° PASS, drift <1° PASS (Oct 13: 0.04° filtered vs 0.26° gyro-only), ≥100 Hz PASS
- Resume numbers: ±2.6° at up to 30° tilt, 200 Hz

### Surprises
- Gyro bias came out different every run (+1.57, -4.17, -2.62 °/s). Cause: setup() moved
  the servo to 90 right before calibrating, and the IMU rides on the card, so the gyro was
  measuring the servo's jump. Fix: delay(1500) after the servo write. Bias dropped to ~0.5 °/s.
- The card "hunts" (twitches back and forth) on some runs. The SG90 moves in 1° steps and
  its gears have slop (backlash), so the loop overshoots and corrects. Raised the deadband
  from 0.3° to 0.5° to reduce it. Not tuning further (non-goal: no PID rabbit hole).
- Chip is mounted a quarter turn on the card, so the angle uses atan2(ay, -ax) instead of
  atan2(ax, ay). The first try read about -90° when level.
- Uploaded sweep.ino by accident at first: the board always runs the last uploaded sketch.
- Placing and removing the phone (to read the tilt) during calibration shook the box.
  Fix: read the tilt, remove the phone, wait 3 s, then start the run.
