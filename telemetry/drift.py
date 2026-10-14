"""
drift.py — measure 60-second drift and loop rate from a plot.py CSV log.

Save as telemetry/drift.py.
Run:  python3 telemetry/drift.py log_YYYYMMDD_HHMMSS.csv
(no file name = it uses the newest log_*.csv in the current folder)

Drift = how far an angle moved over 60 s while the board sat still.
The board didn't really rotate, so any change is error.
"""

import csv
import glob
import sys

SECONDS = 60.0

# Pick the log file: the one you named, or the newest one.
if len(sys.argv) > 1:
    path = sys.argv[1]
else:
    path = sorted(glob.glob("log_*.csv"))[-1]

with open(path) as f:
    rows = list(csv.DictReader(f))

t = [float(r["t_ms"]) / 1000 for r in rows]   # seconds
gyro = [float(r["gyro_angle"]) for r in rows]
filt = [float(r["filt_angle"]) for r in rows]

# Find the first sample at least 60 s after the start.
start = 0
end = next((i for i in range(len(t)) if t[i] - t[start] >= SECONDS), None)
if end is None:
    sys.exit(f"{path}: only {t[-1] - t[0]:.1f} s of data. Need at least {SECONDS:.0f} s.")

duration = t[end] - t[start]
gyro_drift = gyro[end] - gyro[start]
filt_drift = filt[end] - filt[start]

# Loop rate: samples per second over the same window.
loop_hz = (end - start) / duration

print(f"File:            {path}")
print(f"Window:          {duration:.1f} s, {end - start} samples")
print(f"Loop rate:       {loop_hz:.0f} Hz")
print(f"Gyro-only drift: {gyro_drift:+.2f} deg")
print(f"Filtered drift:  {filt_drift:+.2f} deg")
