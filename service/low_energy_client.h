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

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mutex>

#include <base/macros.h>

#include "hal/bluetooth_gatt_interface.h"
#include "service/advertise_data.h"
#include "service/advertise_settings.h"
#include "service/low_energy_constants.h"
#include "service/uuid.h"

namespace bluetooth {

// A LowEnergyClient represents an application's handle to perform various
// Bluetooth Low Energy GAP operations. Instances cannot be created directly and
// should be obtained through the factory.
class LowEnergyClient : private hal::BluetoothGattInterface::ClientObserver {
 public:
  // The destructor automatically unregisters this client instance from the
  // stack.
  ~LowEnergyClient();

  // The app-specific unique ID used while registering this client.
  const UUID& app_identifier() const { return app_identifier_; }

  // The HAL bt_gatt_client "interface ID" assigned to us by the stack. This is
  // what is used internally for BLE transactions.
  int client_if() const { return client_if_; }

  // Callback type used to return the result of asynchronous operations below.
  using StatusCallback = std::function<void(BLEStatus)>;

  // Starts advertising based on the given advertising and scan response
  // data and the provided |settings|. Reports the result of the operation in
  // |callback|.
  bool StartAdvertising(const AdvertiseSettings& settings,
                        const AdvertiseData& advertise_data,
                        const AdvertiseData& scan_response,
                        const StatusCallback& callback);

  // Stops advertising if it was already started. Reports the result of the
  // operation in |callback|.
  bool StopAdvertising(const StatusCallback& callback);

  // Returns true if advertising has been started.
  bool IsAdvertisingStarted() const;

  // Returns the state of pending advertising operations.
  bool IsStartingAdvertising() const;
  bool IsStoppingAdvertising() const;

  // Returns the current advertising settings.
  const AdvertiseSettings& settings() const { return settings_; }

 private:
  friend class LowEnergyClientFactory;

  // Constructor/destructor shouldn't be called directly as instances are meant
  // to be obtained from the factory.
  LowEnergyClient(const UUID& uuid, int client_if);

  // BluetoothGattInterface::ClientObserver overrides:
  void MultiAdvEnableCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int client_if, int status) override;
  void MultiAdvDataCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int client_if, int status) override;
  void MultiAdvDisableCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int client_if, int status) override;

  // Helper method called from SetAdvertiseData/SetScanResponse.
  bt_status_t SetAdvertiseData(
      hal::BluetoothGattInterface* gatt_iface,
      const AdvertiseData& data,
      bool set_scan_rsp);

  // Handles deferred advertise/scan-response data updates. We set the data if
  // there's data to be set, otherwise we either defer it if advertisements
  // aren't enabled or do nothing.
  void HandleDeferredAdvertiseData(hal::BluetoothGattInterface* gatt_iface);

  // Calls and clears the pending callbacks.
  void InvokeAndClearStartCallback(BLEStatus status);
  void InvokeAndClearStopCallback(BLEStatus status);

  // See getters above for documentation.
  UUID app_identifier_;
  int client_if_;

  // Protects advertising-related members below.
  std::mutex adv_fields_lock_;

  // The advertising and scan response data fields that will be sent to the
  // controller.
  AdvertiseData adv_data_;
  AdvertiseData scan_response_;
  std::atomic_bool adv_data_needs_update_;
  std::atomic_bool scan_rsp_needs_update_;

  // Latest advertising settings.
  AdvertiseSettings settings_;

  // Whether or not there is a pending call to update advertising or scan
  // response data.
  std::atomic_bool is_setting_adv_data_;

  std::atomic_bool adv_started_;
  std::unique_ptr<StatusCallback> adv_start_callback_;
  std::unique_ptr<StatusCallback> adv_stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(LowEnergyClient);
};

// LowEnergyClientFactory is used to register and obtain a per-application
// LowEnergyClient instance. Users should call RegisterClient to obtain their
// own unique LowEnergyClient instance that has been registered with the
// Bluetooth stack.
class LowEnergyClientFactory
    : private hal::BluetoothGattInterface::ClientObserver {
 public:
  // Don't construct/destruct directly except in tests. Instead, obtain a handle
  // from an Adapter instance..
  LowEnergyClientFactory();
  ~LowEnergyClientFactory();

  // Registers a LowEnergyClient for the given unique identifier |uuid|. On
  // success, this asynchronously invokes |callback| with a unique pointer to a
  // LowEnergyClient instance whose ownership can be taken by the caller. In the
  // case of an error, the pointer will contain a nullptr.
  using ClientCallback =
      std::function<
          void(BLEStatus, const UUID&, std::unique_ptr<LowEnergyClient>)>;
  bool RegisterClient(const UUID& uuid, const ClientCallback& callback);

 private:
  // BluetoothGattInterface::ClientObserver overrides:
  void RegisterClientCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int client_if,
      const bt_uuid_t& app_uuid) override;

  // Map of pending calls to register.
  std::mutex pending_calls_lock_;
  std::map<UUID, ClientCallback> pending_calls_;

  DISALLOW_COPY_AND_ASSIGN(LowEnergyClientFactory);
};

}  // namespace bluetooth
