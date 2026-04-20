/**
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct JoyConfig {
  struct Button {
    int hid_idx = 0;
  };

  struct AxisHat {
    std::unordered_map<int, float> mapping;
  };

  struct Axis {
    enum class AxisName { X, Y, Z, RX, RY, RZ };
    AxisName axis_name;
    float divisor = 1;
    float offset = 0.0;
  };

  struct AxisInputMapping {
    enum class Type { HAT, AXIS };
    Type type;
    std::variant<AxisHat, Axis> mapping;
  };

  struct ButtonInputMapping {
    enum class Type { EMPTY, BUTTON };
    Type type;
    Button mapping;
  };

  std::string config_name;
  std::vector<AxisInputMapping> axis;
  std::vector<ButtonInputMapping> buttons;
};

JoyConfig parse_config(const std::string& config);
