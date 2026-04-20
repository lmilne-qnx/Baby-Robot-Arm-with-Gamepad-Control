# Joy Gamepad HIDDI

## Overview
This node is a generic gamepad mapper which makes use of hid's gamepad and joystick descriptor to publish results to /joy. This is based on joy_toleop_hiddi so all the button mapping so it should work as a drop in replacement.

## Configuration
As Hid devices all seem to be slightly different a configuration is used to allow correct mapping for the robot arm. two configuration have been provided, one for a generic gamepad which hopefully will work with any generic gamepad, and one for Logitech F710. These configuration can be found in etc.

The configuration is in the JSON format and consists of a name and 2 array mapping to what will be sent over ros2 [sensor_msgs/Joy](https://docs.ros.org/en/api/sensor_msgs/html/msg/Joy.html)
```json
{
  "name": "any_string_name_for_logging",
  // Requires 6 values (left x, left y, right x, right y, dpad x, dpad y)
  "axis": [
      {
         // There are 3 types for Axis "axis", "hat", "empty"
        "type": "axis",
        "value": "x",   // which axis the value is pulled from (x, y, z, rx, ry, rz)
        // Values needed to scale the joystick value from [-1,1] with (value + offset)/divisor
        "divisor": 128,
        "offset": 0
      },
      {
            "type": "hat",
            // Maps HAT value to an output.
            "map": {
                "2": 1,
                "3": 1,
                "4": 1,
                "6": -1,
                "7": -1,
                "8": -1
            }
        }
 ],
 // Buttons are the face buttons and trigger.
 // This requires 12 values where there are 2 types "button" or "empty" where empty means unsupported.
 "button": [
    // Value is the buttons hid value
    {"type": "button", "value": 1},
 ]
}
```

## Running
```
ros2 run joy_gamepad_hiddi joy_gamepad_node --ros-args -p config:=generic_gamepad.json
```
