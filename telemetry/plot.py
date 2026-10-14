"""
plot.py — live plot + CSV log of the Arduino's serial stream.

Save as telemetry/plot.py in your repo.
Setup (once):   pip3 install pyserial matplotlib
Run:            python3 telemetry/plot.py
Close the Arduino Serial Monitor first — only one program can own the port.
Stop with Ctrl+C or by closing the plot window; the CSV is saved either way.
"""

import csv
import time
from collections import deque

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# ---- Settings you might change ----
PORT = "/dev/cu.usbmodem11301"   # check with: ls /dev/cu.usbmodem*
BAUD = 115200
COLUMNS = ["t_ms", "gx", "accel_angle", "gyro_angle", "filt_angle"]  # must match the Arduino's CSV order
PLOT_COLUMNS = ["accel_angle", "gyro_angle", "filt_angle"]   # which ones to draw
WINDOW = 300                                            # how many recent samples to show

# ---- Open the serial port and the log file ----
ser = serial.Serial(PORT, BAUD, timeout=0.05)  # wait at most 50 ms for a line
time.sleep(2)                 # opening the port resets the Mega; give it time to boot
ser.reset_input_buffer()      # throw away half-printed startup lines

log_name = time.strftime("log_%Y%m%d_%H%M%S.csv")
log_file = open(log_name, "w", newline="")
writer = csv.writer(log_file)
writer.writerow(COLUMNS)
print(f"Logging to {log_name}")

# One rolling buffer per column. deque(maxlen=N) drops the oldest value automatically.
data = {name: deque(maxlen=WINDOW) for name in COLUMNS}
lines_read = 0     # every line that arrived
lines_used = 0     # lines that were exactly 7 numbers
last_status = time.time()


def read_available_lines():
    """Read the lines waiting on the port (up to 100), parse them, log them."""
    global lines_read, lines_used
    for _ in range(100):
        raw_bytes = ser.readline()    # returns b"" if nothing arrives within 50 ms
        if not raw_bytes:
            break                     # caught up, go draw
        lines_read += 1
        parts = raw_bytes.decode("utf-8", errors="ignore").strip().split(",")
        if len(parts) != len(COLUMNS):
            continue          # skip text lines like the header or partial lines
        try:
            values = [float(p) for p in parts]
        except ValueError:
            continue          # skip the header line and anything not all numbers
        writer.writerow(values)
        lines_used += 1
        for name, value in zip(COLUMNS, values):
            data[name].append(value)


# ---- Set up the plot ----
fig, ax = plt.subplots()
lines = {name: ax.plot([], [], label=name)[0] for name in PLOT_COLUMNS}
ax.set_xlabel("time (s)")
ax.set_ylabel("angle (deg)")
ax.legend(loc="upper left")
ax.grid(True)


def update(_frame):
    global last_status
    read_available_lines()
    if time.time() - last_status > 2:   # every 2 s, report in Terminal
        print(f"lines read: {lines_read}, plotted: {lines_used}")
        last_status = time.time()
    if not data["t_ms"]:
        return lines.values()
    t = [ms / 1000 for ms in data["t_ms"]]   # milliseconds -> seconds
    for name, line in lines.items():
        line.set_data(t, data[name])
    ax.relim()            # recompute the data limits
    ax.autoscale_view()   # rescale axes to fit
    return lines.values()


anim = FuncAnimation(fig, update, interval=50, cache_frame_data=False)  # redraw every 50 ms

try:
    plt.show()
except KeyboardInterrupt:
    pass
finally:
    log_file.close()
    ser.close()
    print(f"Saved {log_name}")
