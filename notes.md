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
