//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "service/adapter.h"

#include <base/logging.h>

#include "service/logging_helpers.h"

namespace bluetooth {

Adapter::Adapter() : state_(ADAPTER_STATE_OFF) {
  hal::BluetoothInterface::Get()->AddObserver(this);
  hal::BluetoothInterface::Get()->GetHALInterface()->get_adapter_properties();
}

Adapter::~Adapter() {
  hal::BluetoothInterface::Get()->RemoveObserver(this);
}

AdapterState Adapter::GetState() const {
  return state_.load();
}

bool Adapter::IsEnabled() const {
  return state_.load() == ADAPTER_STATE_ON;
}

bool Adapter::Enable() {
  AdapterState current_state = GetState();
  if (current_state != ADAPTER_STATE_OFF) {
    LOG(INFO) << "Adapter not disabled - state: "
              << AdapterStateToString(current_state);
    return false;
  }

  // Set the state before calling enable() as there might be a race between here
  // and the AdapterStateChangedCallback.
  state_ = ADAPTER_STATE_TURNING_ON;

  int status = hal::BluetoothInterface::Get()->GetHALInterface()->enable();
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to enable Bluetooth - status: "
               << BtStatusText((const bt_status_t)status);
    state_ = ADAPTER_STATE_OFF;
    return false;
  }

  // TODO(armansito): Notify others of the state change.

  return true;
}

bool Adapter::Disable() {
  if (!IsEnabled()) {
    LOG(INFO) << "Adapter is not enabled";
    return false;
  }

  AdapterState current_state = GetState();

  // Set the state before calling enable() as there might be a race between here
  // and the AdapterStateChangedCallback.
  state_ = ADAPTER_STATE_TURNING_OFF;

  int status = hal::BluetoothInterface::Get()->GetHALInterface()->disable();
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to disable Bluetooth - status: "
               << BtStatusText((const bt_status_t)status);
    state_ = current_state;
    return false;
  }

  // TODO(armansito): Notify others of the state change.

  return true;
}

bool Adapter::SetName(const std::string& name) {
  bt_bdname_t hal_name;
  size_t max_name_len = sizeof(hal_name.name);

  // Include the \0 byte in size measurement.
  if (name.length() >= max_name_len) {
    LOG(ERROR) << "Given name \"" << name << "\" is larger than maximum allowed"
               << " size: " << max_name_len;
    return false;
  }

  strncpy(reinterpret_cast<char*>(hal_name.name), name.c_str(),
          name.length() + 1);

  VLOG(1) << "Setting adapter name: " << name;

  if (!SetAdapterProperty(BT_PROPERTY_BDNAME, &hal_name, sizeof(hal_name))) {
    LOG(ERROR) << "Failed to set adapter name: " << name;
    return false;
  }

  return true;
}

void Adapter::AdapterStateChangedCallback(bt_state_t state) {
  LOG(INFO) << "Adapter state changed: " << BtStateText(state);

  switch (state) {
  case BT_STATE_OFF:
    state_ = ADAPTER_STATE_OFF;
    break;

  case BT_STATE_ON:
    state_ = ADAPTER_STATE_ON;
    break;

  default:
    NOTREACHED();
  }

  // TODO(armansito): Notify others of the state change.
}

void Adapter::AdapterPropertiesCallback(bt_status_t /* status */,
                                        int /* num_properties */,
                                        bt_property_t* /* properties */) {
  // TODO(armansito): Do something meaningful here.
}

bool Adapter::SetAdapterProperty(bt_property_type_t type,
                                 void* value, int length) {
  CHECK(length > 0);
  CHECK(value);

  bt_property_t property;
  property.len = length;
  property.val = value;
  property.type = type;

  int status = hal::BluetoothInterface::Get()->GetHALInterface()->
      set_adapter_property(&property);
  if (status != BT_STATUS_SUCCESS) {
    VLOG(1) << "Failed to set property";
    return false;
  }

  return true;
}

}  // namespace bluetooth
