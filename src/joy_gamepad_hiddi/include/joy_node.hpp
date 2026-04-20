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

#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joy.hpp>

#include "generic_gamepad.hpp"
#include "joy_config.hpp"

class JoyNode : public rclcpp::Node {
 public:
  JoyNode();

  void sendReport(const GenericGamepad::GamepadData& data);
  void deviceConnect(const GenericGamepad::GamepadInfo& info);
  void deviceDisconnect(hidd_device_instance_t* data);

 private:
  void parse_config(const std::string& config);

 private:
  JoyConfig cfg_;
  bool parsed_config = false;
  rclcpp::Publisher<sensor_msgs::msg::Joy>::SharedPtr publisher_;
  std::chrono::steady_clock::time_point last_update_ = std::chrono::steady_clock::now();
};
