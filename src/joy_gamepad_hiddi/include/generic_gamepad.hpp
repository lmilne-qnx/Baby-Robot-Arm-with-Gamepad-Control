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

#include <sys/hiddi.h>
#include <sys/hidut.h>

#include <functional>

class GenericGamepad {
 public:
  struct GamepadData {
    float x = 0;
    float y = 0;
    float z = 0;
    float rx = 0;
    float ry = 0;
    float rz = 0;
    uint32_t hat = 0;
    std::vector<int> buttons{};
  };

  /// Info relating to connected device
  struct GamepadInfo {
    GenericGamepad *gamepad = 0;
    struct hidd_report_instance *instance = nullptr;
    struct hidd_report *report = nullptr;
    hidd_device_instance_t *device_instance = nullptr;
    GenericGamepad::GamepadData data{};
  };

  using GamepadUpdateCb = std::function<void(const GamepadData &data)>;
  using GamepadConnectCb = std::function<void(const GamepadInfo &data)>;
  using GamepadDisconnectCb = std::function<void(hidd_device_instance_t *data)>;

 public:
  static GenericGamepad &instance() {
    static GenericGamepad gamepad;
    return gamepad;
  }
  ~GenericGamepad();

  inline void set_update_cb(GamepadUpdateCb update_cb) { update_cb_ = update_cb; }
  inline void set_connect_cb(GamepadConnectCb connect_cb) { connect_cb_ = connect_cb; }
  inline void set_disconnect_cb(GamepadDisconnectCb disconnect_cb) { disconnect_cb_ = disconnect_cb; }
  std::string get_device_info(hidd_device_instance_t *data);

 private:
  GenericGamepad();
  static void on_hidd_insert(struct hidd_connection *connection, hidd_device_instance_t *inst);
  static int attach_gamepad(struct hidd_connection *connection, hidd_device_instance_t *inst,
                            struct hidd_collection *collection);
  static void on_hidd_remove(struct hidd_connection *connection, hidd_device_instance_t *inst);
  static void on_hidd_report(struct hidd_connection *connection, struct hidd_report *, void *report_data, uint32_t len,
                             uint32_t, void *user_data);

 private:
  GamepadUpdateCb update_cb_{};
  GamepadConnectCb connect_cb_{};
  GamepadDisconnectCb disconnect_cb_{};

  struct hidd_connection *hid_conn_ = nullptr;
};
