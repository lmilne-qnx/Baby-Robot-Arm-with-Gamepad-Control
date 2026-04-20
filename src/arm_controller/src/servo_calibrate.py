#!/usr/bin/python3

import sys
import PCA9685 as PCA9685
import time


# Base: 25 (restricted left), Shoulder/Elbow: 0 (full forward), Gripper: 15 (closed)
SERVO_MIN_LIMITS = [25.0, 0.0, 50.0, 0.0, 0.0, 15.0]
SERVO_MIN_PULSE = [570, 750, 606, 570, 606, 620]
# Base: 75 (restricted right), Shoulder/Elbow: 50 (stops at upright), Gripper: 65 (open)
SERVO_MAX_LIMITS = [75.0, 50.0, 100.0, 100.0, 100.0, 65.0]
SERVO_MAX_PULSE = [1060, 1060, 1020, 1010, 1020, 930]


pwm = PCA9685.PCA9685()
pwm.set_pwm_freq(50)

if len(sys.argv) < 2:
    print("usage: python3 servo_calibrate.py <servo> [h|l]")
    print("    servo: 0-5 which servo to calibrate")
    print("    level: h for max, l for min, c for center (default center)")
    exit(0)

servo = int(sys.argv[1])
if servo < 0 or servo > 5:
    print("Invalid servo_num expected [0-5], got " + servo)
    exit(1)

# First disable all servos
for i in range(5):
    pwm.set_pwm(i, 0, 0)


min_pulse = SERVO_MIN_PULSE[servo] + (SERVO_MAX_PULSE[servo] - SERVO_MIN_PULSE[servo]) * SERVO_MIN_LIMITS[servo]/100
max_pulse = SERVO_MIN_PULSE[servo] + (SERVO_MAX_PULSE[servo] - SERVO_MIN_PULSE[servo]) * SERVO_MAX_LIMITS[servo]/100
center = SERVO_MIN_PULSE[servo] + (SERVO_MAX_PULSE[servo] - SERVO_MIN_PULSE[servo]) / 2

if len(sys.argv) >= 3:
  if (sys.argv[2] == 'h'):
      value = max_pulse
  elif (sys.argv[2] == 'l'):
      value = min_pulse
  elif (sys.argv[2] == 'c'):
      value = center
  else:
      print("Invalid level, expected l or h")
      exit(1)
else:
      # center
      value = center
value = int(value)

print("setting servo to " + str(value))
pwm.set_pwm(int(sys.argv[1]), 500, value)

# After 2 seconds disable to servo so we don't burn it out
time.sleep(2)
print("shutting servo down")
pwm.set_pwm(int(sys.argv[1]), 0, 0)
