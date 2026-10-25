import matplotlib
matplotlib.use('TkAgg')

import serial
import time
import threading
import numpy as np
import matplotlib.pyplot as plt

SERIAL_PORT = 'COM6'
BAUD_RATE = 9600
authorized_cards = ["0013004466"]
scanned_uid = ""

def get_rfid():
    global scanned_uid
    while True:
        scanned_uid = input("\nTap RFID card (or 'q' to quit): ").strip().upper()

threading.Thread(target=get_rfid, daemon=True).start()

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
time.sleep(2)
ser.reset_input_buffer()
ser.write(b'D')

plt.ion()
fig, ax = plt.subplots()
x_vals, y_vals = [], []

def check_circle(x_list, y_list):
    if len(x_list) < 10:
        return False
    dx = np.array(x_list) - np.mean(x_list)
    dy = np.array(y_list) - np.mean(y_list)
    radius = np.mean(np.hypot(dx, dy))
    angles = np.unwrap(np.arctan2(dy, dx))
    total_angle = np.degrees(np.ptp(angles))
    print(f"Radius: {radius:.0f}, Angle: {total_angle:.0f} deg")
    return radius > 1000 and total_angle > 160

try:
    while True:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            values = line.split(',')
            if len(values) >= 2:
                try:
                    ax_val = int(values[0])
                    ay_val = int(values[1])
                    x_vals.append(ax_val)
                    y_vals.append(ay_val)
                    x_vals = x_vals[-30:]
                    y_vals = y_vals[-30:]

                    ax.clear()
                    ax.plot(x_vals, y_vals, 'ro-')
                    ax.set_xlim(-40000, 40000)
                    ax.set_ylim(-40000, 40000)
                    ax.set_title("MPU6050 Real-Time Motion")
                    ax.grid(True)
                except ValueError:
                    pass

        plt.pause(0.01)

        if scanned_uid:
            uid = scanned_uid
            scanned_uid = ""

            if uid == 'Q':
                break
            elif uid in authorized_cards:
                print("RFID authorized! Perform circular motion for 3 seconds...")
                x_vals, y_vals = [], []
                ser.reset_input_buffer()
                start_time = time.time()

                while time.time() - start_time < 3.0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        values = line.split(',')
                        if len(values) >= 2:
                            try:
                                x_vals.append(int(values[0]))
                                y_vals.append(int(values[1]))
                                ax.clear()
                                ax.plot(x_vals, y_vals, 'ro-')
                                ax.set_xlim(-40000, 40000)
                                ax.set_ylim(-40000, 40000)
                                ax.set_title("Recording Circular Motion...")
                                ax.grid(True)
                            except ValueError:
                                pass
                    plt.pause(0.01)

                if check_circle(x_vals, y_vals):
                    print("Motion verified. Access granted.")
                    ser.write(b'A')
                else:
                    print("Motion invalid. Access denied.")
                    ser.write(b'D')
            else:
                print("UID unauthorized. Access denied.")
                ser.write(b'D')

except KeyboardInterrupt:
    print("Program terminated.")
finally:
    ser.write(b'D')
    ser.close()
    plt.ioff()
    plt.close('all')