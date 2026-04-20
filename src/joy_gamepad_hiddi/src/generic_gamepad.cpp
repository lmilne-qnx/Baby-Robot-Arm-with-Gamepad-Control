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

#include "generic_gamepad.hpp"

#include <iomanip>
#include <sstream>

GenericGamepad::GenericGamepad() {
  // Setup hid
  hidd_device_ident_t interest = {
      HIDD_CONNECT_WILDCARD,
      HIDD_CONNECT_WILDCARD,
      HIDD_CONNECT_WILDCARD,
  };
  static hidd_funcs_t hid_funcs = {
      .nentries = _HIDDI_NFUNCS,
      .insertion = on_hidd_insert,  // Called on device connection
      .removal = on_hidd_remove,    // Called on device disconnection
      .report = on_hidd_report      // Called when device sends data
  };
  hidd_connect_parm_t parm = {NULL, HID_VERSION, HIDD_VERSION, 0, 0, 0, 0, HIDD_CONNECT_WAIT};
  parm.funcs = &hid_funcs;
  parm.device_ident = &interest;

  int status = EOK;
  if ((status = hidd_connect(&parm, &hid_conn_)) != EOK) {
    fprintf(stderr, "%s: Can't connect to HID Server: %s\n", __FUNCTION__, strerror(status));
    exit(1);
  }
}

GenericGamepad::~GenericGamepad() { hidd_disconnect(hid_conn_); }

void GenericGamepad::on_hidd_insert(struct hidd_connection *connection, hidd_device_instance_t *device_instance) {
  struct hidd_collection **hidd_collections, **hidd_mcollections;
  uint16_t num_collections = 0;
  hidd_get_collections(device_instance, NULL, &hidd_collections, &num_collections);

  for (int i = 0; i < num_collections; i++) {
    uint16_t usage_page = 0;
    uint16_t usage = 0;
    hidd_collection_usage(hidd_collections[i], &usage_page, &usage);

    // Only look for joystick or gamepads so only check the root device if it is a joystick or gamepad
    if (usage_page == HIDD_PAGE_DESKTOP && (usage == HIDD_USAGE_JOYSTICK || usage == HIDD_USAGE_GAMEPAD)) {
      // Try and attach the root device
      if (attach_gamepad(connection, device_instance, hidd_collections[i]) == EOK) {
        return;
      }
    } else {
      printf("Invalid usage page %d %d\n", (int)usage_page, (int)usage);
    }

    // HID devices can also have nested devices so also check those
    struct hidd_collection **nested_collections;
    uint16_t num_nested_collections;
    int status = hidd_get_collections(NULL, hidd_collections[i], &nested_collections, &num_nested_collections);
    if (status != EOK) {  // No nested collections
      continue;
    }

    for (int j = 0; j < num_nested_collections; j++) {
      hidd_collection_usage(nested_collections[j], &usage_page, &usage);

      // Only look for joystick or gamepads
      if (usage_page != HIDD_PAGE_DESKTOP || !(usage == HIDD_USAGE_JOYSTICK || usage == HIDD_USAGE_GAMEPAD)) {
        printf("Invalid usage page(nested) %d %d\n", (int)usage_page, (int)usage);
      }
      if (attach_gamepad(connection, device_instance, nested_collections[i]) == EOK) {
        return;
      }
    }
  }
}

int GenericGamepad::attach_gamepad(struct hidd_connection *connection, hidd_device_instance_t *device_instance,
                                   struct hidd_collection *collection) {
  struct hidd_report_instance *report_instance;
  struct hidd_report *report;
  int status = hidd_get_report_instance(collection, 0, HID_INPUT_REPORT, &report_instance);
  if (status != EOK) {
    return status;
  }
  uint16_t button_count = 0;
  hidd_num_buttons(report_instance, &button_count);

  // Try to attach the device
  status = hidd_report_attach(connection, device_instance, report_instance, 0, sizeof(GamepadInfo), &report);
  if (status != EOK) {
    return status;
  }

  // Got a Device!
  GamepadInfo *gamepad_info = (GamepadInfo *)hidd_report_extra(report);
  gamepad_info->data.buttons.resize(button_count);
  gamepad_info->device_instance = device_instance;
  gamepad_info->report = report;
  gamepad_info->instance = report_instance;
  gamepad_info->gamepad = &GenericGamepad::instance();
  if (GenericGamepad::instance().connect_cb_) {
    GenericGamepad::instance().connect_cb_(*gamepad_info);
  }
  return EOK;
}

std::string GenericGamepad::get_device_info(hidd_device_instance_t *device_instance) {
  char buffer[50];
  const char *manufacturer, *product, *serial_number;
  struct hidd_connection *connection = hid_conn_;

  manufacturer = (hidd_get_manufacturer_string(connection, device_instance, buffer, 50) == EOK) ? buffer : "";
  product = (hidd_get_product_string(connection, device_instance, buffer, 50) == EOK) ? buffer : "";
  serial_number = (hidd_get_serial_number_string(connection, device_instance, buffer, 50) == EOK) ? buffer : "";

  std::stringstream ss;
  ss << "Device Address=" << device_instance->devno;
  ss << " Vendor=0x" << std::hex << std::setfill('0') << std::setw(4) << device_instance->device_ident.vendor_id << " ("
     << manufacturer << ")";
  ss << " Product=0x" << std::hex << std::setfill('0') << std::setw(4) << device_instance->device_ident.product_id
     << "(" << product << ")";
  ss << " Version=r" << (device_instance->device_ident.version >> 8) << "."
     << (device_instance->device_ident.version & 0xFF);
  return ss.str();
}

void GenericGamepad::on_hidd_remove(struct hidd_connection *conn, hidd_device_instance_t *inst) {
  if (GenericGamepad::instance().disconnect_cb_) {
    GenericGamepad::instance().disconnect_cb_(inst);
  }
  hidd_reports_detach(conn, inst);
}

void GenericGamepad::on_hidd_report(struct hidd_connection *conn, struct hidd_report *, void *report_data, uint32_t len,
                                    uint32_t, void *user_data) {
  GamepadInfo *info = (GamepadInfo *)user_data;
  if (!info) return;

  int x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0;
  hidd_get_usage_value(info->instance, NULL, 1, HIDD_USAGE_X, report_data, (uint32_t *)&x);
  hidd_get_usage_value(info->instance, NULL, 1, HIDD_USAGE_Y, report_data, (uint32_t *)&y);
  hidd_get_usage_value(info->instance, NULL, 1, HIDD_USAGE_Z, report_data, (uint32_t *)&z);
  hidd_get_usage_value(info->instance, NULL, 1, HIDD_USAGE_RX, report_data, (uint32_t *)&rx);
  hidd_get_usage_value(info->instance, NULL, 1, HIDD_USAGE_RY, report_data, (uint32_t *)&ry);
  hidd_get_usage_value(info->instance, NULL, 1, HIDD_USAGE_RZ, report_data, (uint32_t *)&rz);

  // Have some dead zones to stop gitter.
  info->data.x = (std::abs(x) > 10) ? x : 0;
  info->data.y = (std::abs(y) > 10) ? y : 0;
  info->data.z = (std::abs(z) > 10) ? z : 0;
  info->data.rx = (std::abs(rx) > 10) ? rx : 0;
  info->data.ry = (std::abs(ry) > 10) ? ry : 0;
  info->data.rz = (std::abs(rz) > 10) ? rz : 0;

  uint32_t dpad = 0;
  hidd_get_usage_value(info->instance, NULL, 1, HIDD_USAGE_HAT_SWITCH, report_data, &info->data.hat);

  // Reset buttons
  std::fill(info->data.buttons.begin(), info->data.buttons.end(), 0);
  std::uint16_t num_press = info->data.buttons.size();
  std::uint16_t button_usages[num_press];
  hidd_get_buttons(info->instance, NULL, HIDD_PAGE_BUTTONS, report_data, button_usages, &num_press);
  for (int i = 0; i < num_press; i++) {
    if (button_usages[i] < info->data.buttons.size()) {
      info->data.buttons[button_usages[i]] = 1;
    }
  }

  if (info->gamepad->update_cb_) {
    info->gamepad->update_cb_(info->data);
  }
}
