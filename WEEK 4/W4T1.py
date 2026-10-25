import time
from collections import deque

import numpy as np
import serial
import matplotlib.pyplot as plt

SERIAL_PORT = '/dev/cu.usbmodemFX2348N1' #Using macbook IOS
BAUD_RATE = 9600
MAX_POINTS = 35

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
time.sleep(2)
ser.reset_input_buffer()

x_vals = deque(maxlen=MAX_POINTS)
y_vals = deque(maxlen=MAX_POINTS)

plt.ion()
fig, ax = plt.subplots(figsize=(7, 7))
ax.set_xlim(-20000, 20000)
ax.set_ylim(-20000, 20000)
ax.set_title("Task 1: Real-Time MPU6050 Motion Tracking")
ax.set_xlabel("Acceleration X (raw LSB)")
ax.set_ylabel("Acceleration Y (raw LSB)")
ax.grid(True, linestyle='--', alpha=0.6)

line, = ax.plot([], [], 'ro-', linewidth=2, markersize=5, label='Hand path')
center_dot, = ax.plot([], [], 'bx', markersize=10, label='Circle centre')
status_box = ax.text(-18500, 17500, "Status: Collecting data...",
                     fontsize=11, fontweight='bold',
                     bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
ax.legend(loc='lower right')

def check_circular_gesture(x_arr, y_arr):
    if len(x_arr) < 16:
        return False, 0, 0

    x = np.array(x_arr, dtype=float)
    y = np.array(y_arr, dtype=float)

    cx, cy = np.mean(x), np.mean(y)

    span_x = np.max(x) - np.min(x)
    span_y = np.max(y) - np.min(y)
    if span_x < 6000 or span_y < 6000:
        return False, cx, cy

    dx = x - cx
    dy = y - cy
    q1 = np.any((dx > 0) & (dy > 0))
    q2 = np.any((dx < 0) & (dy > 0))
    q3 = np.any((dx < 0) & (dy < 0))
    q4 = np.any((dx > 0) & (dy < 0))
    if not (q1 and q2 and q3 and q4):
        return False, cx, cy

    radii = np.sqrt(dx ** 2 + dy ** 2)
    if (np.std(radii) / np.mean(radii)) >= 0.45:
        return False, cx, cy

    cross = dx[:-1] * dy[1:] - dy[:-1] * dx[1:]
    ccw = np.sum(cross > 0)
    cw = np.sum(cross < 0)
    if max(ccw, cw) / max(ccw + cw, 1) < 0.7:
        return False, cx, cy

    return True, cx, cy

print("Tracking started. Move the sensor in a circle. Press Ctrl+C to exit.")

try:
    while plt.fignum_exists(fig.number):
        try:
            raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not raw_line:
                continue
            values = raw_line.split(',')

            if len(values) < 3:
                continue

            x_vals.append(int(values[0]))
            y_vals.append(int(values[1]))

            if len(x_vals) >= 3:
                kernel = np.ones(3) / 3
                smooth_x = np.convolve(np.array(x_vals), kernel, mode='valid')
                smooth_y = np.convolve(np.array(y_vals), kernel, mode='valid')
            else:
                smooth_x, smooth_y = np.array(x_vals), np.array(y_vals)

            line.set_data(smooth_x, smooth_y)

            detected, cx, cy = check_circular_gesture(smooth_x, smooth_y)
            if detected:
                center_dot.set_data([cx], [cy])
                status_box.set_text("Status: CIRCULAR MOTION DETECTED!")
                status_box.set_bbox(dict(boxstyle='round', facecolor='lightgreen', alpha=0.9))
            else:
                center_dot.set_data([], [])
                status_box.set_text("Status: Tracking motion...")
                status_box.set_bbox(dict(boxstyle='round', facecolor='wheat', alpha=0.8))

            plt.draw()
            plt.pause(0.02)

        except ValueError:

            continue

except KeyboardInterrupt:
    print("\nProgram terminated by user.")
finally:
    ser.close()
    plt.close('all')
    print("Serial connection closed.")