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

// static
const char Adapter::kDefaultAddress[] = "00:00:00:00:00:00";
// static
const char Adapter::kDefaultName[] = "not-initialized";

// The minimum number of advertising instances required for multi-advertisement
// support.
//
// TODO(armansito): This number comes straight from
// packages/apps/Bluetooth/src/c/a/b/btservice/AdapterService.java. It would be
// nice to know if there were a way to obtain this number from the stack instead
// of hardcoding it here.
const char kMinAdvInstancesForMultiAdv = 5;

void Adapter::Observer::OnAdapterStateChanged(Adapter* adapter,
                                              AdapterState prev_state,
                                              AdapterState new_state) {
  // Default implementation does nothing
}

Adapter::Adapter()
    : state_(ADAPTER_STATE_OFF),
      address_(kDefaultAddress),
      name_(kDefaultName) {
  memset(&local_le_features_, 0, sizeof(local_le_features_));
  hal::BluetoothInterface::Get()->AddObserver(this);
  ble_client_factory_.reset(new LowEnergyClientFactory());
  gatt_client_factory_.reset(new GattClientFactory());
  gatt_server_factory_.reset(new GattServerFactory());
  hal::BluetoothInterface::Get()->GetHALInterface()->get_adapter_properties();
}

Adapter::~Adapter() {
  hal::BluetoothInterface::Get()->RemoveObserver(this);
}

void Adapter::AddObserver(Observer* observer) {
  std::lock_guard<std::mutex> lock(observers_lock_);
  observers_.AddObserver(observer);
}

void Adapter::RemoveObserver(Observer* observer) {
  std::lock_guard<std::mutex> lock(observers_lock_);
  observers_.RemoveObserver(observer);
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
  NotifyAdapterStateChanged(current_state, state_);

  int status = hal::BluetoothInterface::Get()->GetHALInterface()->enable();
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to enable Bluetooth - status: "
               << BtStatusText((const bt_status_t)status);
    state_ = ADAPTER_STATE_OFF;
    NotifyAdapterStateChanged(ADAPTER_STATE_TURNING_ON, state_);
    return false;
  }

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
  NotifyAdapterStateChanged(current_state, state_);

  int status = hal::BluetoothInterface::Get()->GetHALInterface()->disable();
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to disable Bluetooth - status: "
               << BtStatusText((const bt_status_t)status);
    state_ = current_state;
    NotifyAdapterStateChanged(ADAPTER_STATE_TURNING_OFF, state_);
    return false;
  }

  return true;
}

std::string Adapter::GetName() const {
  return name_.Get();
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

std::string Adapter::GetAddress() const {
  return address_.Get();
}

bool Adapter::IsMultiAdvertisementSupported() const {
  return local_le_features_.max_adv_instance >= kMinAdvInstancesForMultiAdv;
}

LowEnergyClientFactory* Adapter::GetLowEnergyClientFactory() const {
  return ble_client_factory_.get();
}

GattClientFactory* Adapter::GetGattClientFactory() const {
  return gatt_client_factory_.get();
}

GattServerFactory* Adapter::GetGattServerFactory() const {
  return gatt_server_factory_.get();
}

void Adapter::AdapterStateChangedCallback(bt_state_t state) {
  LOG(INFO) << "Adapter state changed: " << BtStateText(state);

  AdapterState prev_state = GetState();

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

  NotifyAdapterStateChanged(prev_state, GetState());
}

void Adapter::AdapterPropertiesCallback(bt_status_t status,
                                        int num_properties,
                                        bt_property_t* properties) {
  LOG(INFO) << "Adapter properties changed";

  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "status: " << BtStatusText(status);
    return;
  }

  for (int i = 0; i < num_properties; i++) {
    bt_property_t* property = properties + i;
    switch (property->type) {
      case BT_PROPERTY_BDADDR: {
        std::string address = BtAddrString(reinterpret_cast<bt_bdaddr_t*>(
            property->val));
        LOG(INFO) << "Adapter address changed: " << address;
        address_.Set(address);
        break;
      }
      case BT_PROPERTY_BDNAME: {
        bt_bdname_t* hal_name = reinterpret_cast<bt_bdname_t*>(property->val);
        std::string name = reinterpret_cast<char*>(hal_name->name);
        LOG(INFO) << "Adapter name changed: " << name;
        name_.Set(name);
        break;
      }
      case BT_PROPERTY_LOCAL_LE_FEATURES: {
        if (property->len != sizeof(bt_local_le_features_t)) {
          LOG(WARNING) << "Malformed value received for property: "
                       << "BT_PROPERTY_LOCAL_LE_FEATURES";
          break;
        }
        bt_local_le_features_t* features =
            reinterpret_cast<bt_local_le_features_t*>(property->val);
        memcpy(&local_le_features_, features, sizeof(*features));
        LOG(INFO) << "Supported LE features updated";
        break;
      }
      default:
        VLOG(1) << "Unhandled adapter property: "
                << BtPropertyText(property->type);
        break;
    }

    // TODO(armansito): notify others of the updated properties
  }
}

void Adapter::DiscoveryStateChangedCallback(bt_discovery_state_t state) {
  // This does nothing
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

void Adapter::NotifyAdapterStateChanged(AdapterState prev_state,
                                        AdapterState new_state) {
  if (prev_state == new_state)
    return;

  std::lock_guard<std::mutex> lock(observers_lock_);
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnAdapterStateChanged(this, prev_state, new_state));
}

}  // namespace bluetooth
