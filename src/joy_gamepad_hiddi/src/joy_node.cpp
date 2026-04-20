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
#include "joy_node.hpp"

#include <sys/json.h>

#include <sstream>

namespace {

static std::unordered_map<std::string, JoyConfig::Axis::AxisName> s_axis_name_mapping = {
    {"x", JoyConfig::Axis::AxisName::X},   {"y", JoyConfig::Axis::AxisName::Y},   {"z", JoyConfig::Axis::AxisName::Z},
    {"rx", JoyConfig::Axis::AxisName::RX}, {"ry", JoyConfig::Axis::AxisName::RY}, {"rz", JoyConfig::Axis::AxisName::RZ},
};
}

JoyNode::JoyNode() : rclcpp::Node("generic_gamepad") {
  RCLCPP_INFO(get_logger(), "Starting Generic Gamepad Node");

  {
    auto param_desc = rcl_interfaces::msg::ParameterDescriptor{};
    param_desc.name = "config";
    param_desc.description = "Gamepad mapping config [required]";
    param_desc.read_only = true;
    param_desc.type = rclcpp::PARAMETER_STRING;
    this->declare_parameter("config", "world", param_desc);
  }

  publisher_ = this->create_publisher<sensor_msgs::msg::Joy>("/joy", 10);
  GenericGamepad::instance().set_update_cb(std::bind(&JoyNode::sendReport, this, std::placeholders::_1));
  GenericGamepad::instance().set_connect_cb(std::bind(&JoyNode::deviceConnect, this, std::placeholders::_1));
  GenericGamepad::instance().set_disconnect_cb(std::bind(&JoyNode::deviceDisconnect, this, std::placeholders::_1));
  parse_config(this->get_parameter("config").as_string());
  if (!parsed_config) {
    // Config failed no point trying.
    exit(1);
  }
}

void JoyNode::parse_config(const std::string& config) {
  RCLCPP_INFO(get_logger(), "Parsing config %s", config.c_str());

  json_decoder_t* dec = json_decoder_create();
  json_decoder_error_t parse_err = json_decoder_parse_file(dec, config.c_str());
  if (parse_err != JSON_DECODER_OK) {
    RCLCPP_ERROR(get_logger(), "Failed to parse config");
    return;
  }

  if (json_decoder_push_object(dec, nullptr, false) != JSON_DECODER_OK) {
    RCLCPP_ERROR(get_logger(), "parse_config: Missing root JSON object.");
    return;
  }

  const char* name = nullptr;
  if (json_decoder_get_string(dec, "name", &name, false) != JSON_DECODER_OK) {
    RCLCPP_ERROR(get_logger(), "parse_config: Missing or invalid type for name.");
    return;
  }
  cfg_.config_name = name;

  // Axis object
  if (json_decoder_push_array(dec, "axis", false) != JSON_DECODER_OK) {
    RCLCPP_ERROR(get_logger(), "parse_config: Missing 'axis' array");
    return;
  }

  int outer_length = json_decoder_length(dec);
  for (int i = 0; i < outer_length; i++) {
    if (json_decoder_push_object(dec, nullptr, true) != JSON_DECODER_OK) {
      RCLCPP_ERROR(get_logger(), "parse_config: axis array[%d] push object failed", json_decoder_index(dec));
      return;
    }

    const char* type_str = nullptr;
    if (json_decoder_get_string(dec, "type", &type_str, false) != JSON_DECODER_OK) {
      RCLCPP_ERROR(get_logger(), "parse_config: Missing 'type' in axis array[%d]", json_decoder_index(dec));
      return;
    }

    JoyConfig::AxisInputMapping axis;
    if (strcmp(type_str, "axis") == 0) {
      axis.type = JoyConfig::AxisInputMapping::Type::AXIS;
      JoyConfig::Axis axis_mapping;

      const char* value_str = nullptr;
      if (json_decoder_get_string(dec, "value", &value_str, false) != JSON_DECODER_OK) {
        RCLCPP_ERROR(get_logger(), "parse_config: Missing 'value' in axis array[%d]", json_decoder_index(dec));
        return;
      }
      auto mapping = s_axis_name_mapping.find(value_str);
      if (mapping == s_axis_name_mapping.end()) {
        RCLCPP_ERROR(get_logger(), "parse_config: Invalid 'value' in axis array[%d]", json_decoder_index(dec));
        return;
      }
      axis_mapping.axis_name = mapping->second;

      double d_value;
      if (json_decoder_get_double(dec, "divisor", &d_value, true) == JSON_DECODER_OK) {
        axis_mapping.divisor = d_value;
      }
      if (json_decoder_get_double(dec, "offset", &d_value, true) == JSON_DECODER_OK) {
        axis_mapping.offset = d_value;
      }
      axis.mapping = axis_mapping;
    } else if (strcmp(type_str, "hat") == 0) {
      axis.type = JoyConfig::AxisInputMapping::Type::HAT;
      JoyConfig::AxisHat axis_mapping;

      if (json_decoder_push_object(dec, "map", false) != JSON_DECODER_OK) {
        RCLCPP_ERROR(get_logger(), "parse_config: Missing 'map' in axis hat array[%d]", json_decoder_index(dec));
        return;
      }

      int length = json_decoder_length(dec);
      for (int i = 0; i < length; i++) {
        const char* name = json_decoder_name(dec);
        double d_value;
        if (json_decoder_get_double(dec, NULL, &d_value, false) != JSON_DECODER_OK) {
          RCLCPP_ERROR(get_logger(), "parse_config: must be double hat array[%d]", json_decoder_index(dec));
          return;
        }
        axis_mapping.mapping.insert({std::stoi(name), d_value});
      }
      axis.mapping = axis_mapping;
      json_decoder_pop(dec);
    } else {
      RCLCPP_ERROR(get_logger(), "pase_config: Invalid type: %s", type_str);
      return;
    }
    cfg_.axis.push_back(axis);
    json_decoder_pop(dec);  // Axis Element.
  }

  json_decoder_pop(dec);  // Axis Array

  // Buttons
  if (json_decoder_push_array(dec, "button", false) != JSON_DECODER_OK) {
    RCLCPP_ERROR(get_logger(), "parse_config: Missing 'button' array");
    return;
  }

  outer_length = json_decoder_length(dec);
  for (int i = 0; i < outer_length; i++) {
    JoyConfig::ButtonInputMapping mapping;
    json_decoder_push_object(dec, NULL, false);

    const char* type_str = nullptr;
    if (json_decoder_get_string(dec, "type", &type_str, false) != JSON_DECODER_OK) {
      RCLCPP_ERROR(get_logger(), "parse_config: Missing 'type' in axis array[%d]", json_decoder_index(dec));
      return;
    }

    if (strcmp(type_str, "empty") == 0) {
      // no-op
      mapping.type = JoyConfig::ButtonInputMapping::Type::EMPTY;
    } else if (strcmp(type_str, "button") == 0) {
      mapping.type = JoyConfig::ButtonInputMapping::Type::BUTTON;
      int i_value;
      if (json_decoder_get_int(dec, NULL, &i_value, false) != JSON_DECODER_OK) {
        RCLCPP_ERROR(get_logger(), "parse_config: Missing 'value' button[%d]", json_decoder_index(dec));
        return;
      }
      mapping.mapping.hid_idx = i_value;
    }
    cfg_.buttons.push_back(mapping);

    json_decoder_pop(dec);  // Button entry
  }

  json_decoder_pop(dec);  // Buttons object

  json_decoder_destroy(dec);
  parsed_config = true;
}

void JoyNode::sendReport(const GenericGamepad::GamepadData& data) {
  // Only report every 20ms
  auto now = std::chrono::steady_clock::now();
  if (now - last_update_ < std::chrono::milliseconds(20)) {
    return;
  }
  last_update_ = now;

  std::stringstream report_button_ss;
  for (int i = 0; i < data.buttons.size(); i++) {
    if (data.buttons[i]) {
      report_button_ss << i << " ";
    }
  }
  RCLCPP_INFO(get_logger(), "Sending Report: x=%f y=%f z=%f rx=%f ry=%f rz=%f hat=%u buttons=%s\n", data.x, data.y,
              data.z, data.rx, data.ry, data.rz, data.hat, report_button_ss.str().c_str());

  if (data.buttons.size() < 16) {
    RCLCPP_WARN(get_logger(), "Not enough data to send report:%ld", data.buttons.size());
    return;
  }

  auto joy_msg = sensor_msgs::msg::Joy();
  joy_msg.header.stamp = this->get_clock()->now();

  int idx = 0;
  for (JoyConfig::AxisInputMapping& axis : cfg_.axis) {
    switch (axis.type) {
      case JoyConfig::AxisInputMapping::Type::AXIS: {
        JoyConfig::Axis axis_mapping = std::get<JoyConfig::Axis>(axis.mapping);
        float value;
        switch (axis_mapping.axis_name) {
          case JoyConfig::Axis::AxisName::X:
            value = data.x;
            break;
          case JoyConfig::Axis::AxisName::Y:
            value = data.y;
            break;
          case JoyConfig::Axis::AxisName::Z:
            value = data.z;
            break;
          case JoyConfig::Axis::AxisName::RX:
            value = data.rx;
            break;
          case JoyConfig::Axis::AxisName::RY:
            value = data.ry;
            break;
          case JoyConfig::Axis::AxisName::RZ:
            value = data.rz;
            break;
        }
        joy_msg.axes.push_back((value + axis_mapping.offset) / axis_mapping.divisor);
        break;
      }
      case JoyConfig::AxisInputMapping::Type::HAT: {
        JoyConfig::AxisHat axis_mapping = std::get<JoyConfig::AxisHat>(axis.mapping);
        auto itr = axis_mapping.mapping.find(data.hat);
        if (itr != axis_mapping.mapping.end()) {
          printf("mapping %f", itr->second);
          joy_msg.axes.push_back(itr->second);
        } else {
          joy_msg.axes.push_back(0);
        }
        break;
      }
    }
  }

  for (JoyConfig::ButtonInputMapping mapping : cfg_.buttons) {
    switch (mapping.type) {
      case JoyConfig::ButtonInputMapping::Type::BUTTON:
        joy_msg.buttons.push_back(data.buttons[mapping.mapping.hid_idx]);
        break;
      case JoyConfig::ButtonInputMapping::Type::EMPTY:
        joy_msg.buttons.push_back(0);  // Not Implemented
        break;
    }
  }

  // Having 2 info print in this function causes a crash todo figure out why..
  // {
  //   std::stringstream ss;
  //   ss << "Joy sticks: [ ";
  //   for (auto& joy : joy_msg.axes) {
  //     ss << std::to_string(joy) << ", ";
  //   }
  //   ss << "] buttons [";
  //   for (auto& button : joy_msg.buttons) {
  //     ss << std::to_string((int)button) << ", ";
  //   }
  //   ss << "]";
  //   std::string log = ss.str();
  //   RCLCPP_DEBUG(get_logger(), "%s", log.c_str());
  // }
  publisher_->publish(joy_msg);
}

void JoyNode::deviceConnect(const GenericGamepad::GamepadInfo& info) {
  auto device_str = GenericGamepad::instance().get_device_info(info.device_instance);
  RCLCPP_INFO(get_logger(), "Device Connected: %s", device_str.c_str());
}
void JoyNode::deviceDisconnect(hidd_device_instance_t* dev_inst) {
  auto device_str = GenericGamepad::instance().get_device_info(dev_inst);
  RCLCPP_INFO(get_logger(), "Device Disconnected: %s", device_str.c_str());
}
