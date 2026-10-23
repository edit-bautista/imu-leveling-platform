"""
log_level.py — record one leveling test and print the resume numbers.

Usage (from the IMU Project folder, Arduino Serial Monitor CLOSED):
    python3 telemetry/log_level.py 20

    20 = how far the BOX is tilted, in degrees, read from the phone's Level.

What it does:
  1. Opens the serial port. This resets the Mega, so it recalibrates the gyro
     (keep the box still!) and the loop starts again from servo command 90.
  2. Records 15 seconds of CSV lines: t_ms,accel_deg,filt_deg,servo_cmd
  3. Ignores the first 5 seconds (the loop is still swinging the card to level).
  4. Over the last 10 seconds it reports:
       - leveling error: the worst |filt_deg| (how far from level the card got)
       - mean angle: where the card settled on average
       - loop rate in Hz, from the t_ms timestamps
  5. Saves everything to logs/level_tilt<N>.csv
"""

import csv
import glob
import os
import sys

import serial  # pyserial

RECORD_S = 15   # total seconds to record
SETTLE_S = 5    # seconds to skip while the loop settles

tilt = sys.argv[1] if len(sys.argv) > 1 else "0"

ports = glob.glob("/dev/cu.usbmodem*")
if not ports:
    sys.exit("No Arduino found. Is the USB plugged in?")
ser = serial.Serial(ports[0], 115200, timeout=2)

print(f"Box tilt = {tilt} deg. Keep the box STILL while the gyro calibrates...")

# Wait for the CSV header, which prints after calibration finishes
while True:
    line = ser.readline().decode(errors="ignore").strip()
    if line.startswith("#"):
        print(line)                      # WHO_AM_I and gyro bias lines
    if line.startswith("t_ms"):
        break

print(f"Recording {RECORD_S} s...")
rows = []
while True:
    line = ser.readline().decode(errors="ignore").strip()
    parts = line.split(",")
    if len(parts) != 4:
        continue                         # skip partial or garbled lines
    try:
        t, accel, filt, cmd = int(parts[0]), float(parts[1]), float(parts[2]), float(parts[3])
    except ValueError:
        continue
    rows.append((t, accel, filt, cmd))
    if t - rows[0][0] >= RECORD_S * 1000:
        break
ser.close()

# Save the full log
os.makedirs("logs", exist_ok=True)
path = f"logs/level_tilt{tilt}.csv"
with open(path, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["t_ms", "accel_deg", "filt_deg", "servo_cmd"])
    w.writerows(rows)

# Analyze only the settled part
start = rows[0][0] + SETTLE_S * 1000
settled = [r for r in rows if r[0] >= start]
filt = [r[2] for r in settled]
times = [r[0] for r in settled]

worst = max(abs(a) for a in filt)
mean = sum(filt) / len(filt)
rate = (len(times) - 1) / ((times[-1] - times[0]) / 1000.0)

print()
print(f"Saved {len(rows)} lines to {path}")
print(f"Box tilt:          {tilt} deg")
print(f"Leveling error:    +/-{worst:.2f} deg  (worst |filt_deg|, last {RECORD_S - SETTLE_S} s)")
print(f"Mean angle:        {mean:+.2f} deg")
print(f"Servo command:     {settled[-1][3]:.1f}")
print(f"Loop rate:         {rate:.0f} Hz")
