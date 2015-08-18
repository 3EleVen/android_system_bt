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

#include "service/hal/bluetooth_interface.h"

#include <mutex>

#include <base/logging.h>
#include <base/observer_list.h>

#include "service/logging_helpers.h"

extern "C" {
#include "btcore/include/hal_util.h"
}  // extern "C"

using std::lock_guard;
using std::mutex;

namespace bluetooth {
namespace hal {

namespace {

// The global BluetoothInterface instance.
BluetoothInterface* g_bluetooth_interface = nullptr;

// Mutex used by callbacks to access |g_bluetooth_interface|. Since there is no
// good way to unregister callbacks and since the global instance can be deleted
// concurrently during shutdown, this lock is used.
//
// TODO(armansito): There should be a way to cleanly shut down the Bluetooth
// stack.
mutex g_instance_lock;

// Helper for obtaining the observer list. This is forward declared here and
// defined below since it depends on BluetoothInterfaceImpl.
base::ObserverList<BluetoothInterface::Observer>* GetObservers();

#define FOR_EACH_BLUETOOTH_OBSERVER(func) \
  FOR_EACH_OBSERVER(BluetoothInterface::Observer, *GetObservers(), func)

void AdapterStateChangedCallback(bt_state_t state) {
  lock_guard<mutex> lock(g_instance_lock);
  if (!g_bluetooth_interface) {
    LOG(WARNING) << "Callback received after global instance was destroyed";
    return;
  }

  VLOG(1) << "Adapter state changed: " << BtStateText(state);
  FOR_EACH_BLUETOOTH_OBSERVER(AdapterStateChangedCallback(state));
}

void AdapterPropertiesCallback(bt_status_t status,
                               int num_properties,
                               bt_property_t* properties) {
  lock_guard<mutex> lock(g_instance_lock);
  if (!g_bluetooth_interface) {
    LOG(WARNING) << "Callback received after global instance was destroyed";
    return;
  }

  VLOG(1) << "Adapter properties changed - status: " << BtStatusText(status)
          << ", num_properties: " << num_properties;
  FOR_EACH_BLUETOOTH_OBSERVER(
      AdapterPropertiesCallback(status, num_properties, properties));
}

void ThreadEventCallback(bt_cb_thread_evt evt) {
  VLOG(1) << "ThreadEventCallback" << BtEventText(evt);

  // TODO(armansito): This callback is completely useless to us but btif borks
  // out if this is not set. Consider making this optional.
}

bool SetWakeAlarmCallout(uint64_t /* delay_millis */,
                         bool /* should_wake */,
                         alarm_cb /* cb */,
                         void* /* data */) {
  // TODO(armansito): Figure out what to do with this callback. It's not being
  // used by us right now but the code crashes without setting it. The stack
  // should be refactored to make things optional and definitely not crash.
  // (See http://b/23315739)
  return true;
}

int AcquireWakeLockCallout(const char* /* lock_name */) {
  // TODO(armansito): Figure out what to do with this callback. It's not being
  // used by us right now but the code crashes without setting it. The stack
  // should be refactored to make things optional and definitely not crash.
  // (See http://b/23315739)
  return BT_STATUS_SUCCESS;
}

int ReleaseWakeLockCallout(const char* /* lock_name */) {
  // TODO(armansito): Figure out what to do with this callback. It's not being
  // used by us right now but the code crashes without setting it. The stack
  // should be refactored to make things optional and definitely not crash.
  // (See http://b/23315739)
  return BT_STATUS_SUCCESS;
}

// The HAL Bluetooth DM callbacks.
bt_callbacks_t bt_callbacks = {
  sizeof(bt_callbacks_t),
  AdapterStateChangedCallback,
  AdapterPropertiesCallback,
  nullptr, /* remote_device_properties_cb */
  nullptr, /* device_found_cb */
  nullptr, /* discovery_state_changed_cb */
  nullptr, /* pin_request_cb  */
  nullptr, /* ssp_request_cb  */
  nullptr, /* bond_state_changed_cb */
  nullptr, /* acl_state_changed_cb */
  ThreadEventCallback,
  nullptr, /* dut_mode_recv_cb */
  nullptr, /* le_test_mode_cb */
  nullptr  /* energy_info_cb */
};

bt_os_callouts_t bt_os_callouts = {
  sizeof(bt_os_callouts_t),
  SetWakeAlarmCallout,
  AcquireWakeLockCallout,
  ReleaseWakeLockCallout
};

}  // namespace

// BluetoothInterface implementation for production.
class BluetoothInterfaceImpl : public BluetoothInterface {
 public:
  BluetoothInterfaceImpl()
      : hal_iface_(nullptr),
        hal_adapter_(nullptr) {
  }

  ~BluetoothInterfaceImpl() override {
    hal_iface_->cleanup();
  };

  // BluetoothInterface overrides.
  void AddObserver(Observer* observer) override {
    lock_guard<mutex> lock(g_instance_lock);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    lock_guard<mutex> lock(g_instance_lock);
    observers_.RemoveObserver(observer);
  }

  const bt_interface_t* GetHALInterface() const override {
    return hal_iface_;
  }

  const bluetooth_device_t* GetHALAdapter() const override {
    return hal_adapter_;
  }

  // Initialize the interface. This loads the shared Bluetooth library and sets
  // up the callbacks.
  bool Initialize() {
    // Load the Bluetooth shared library module.
    const hw_module_t* module;
    int status = hal_util_load_bt_library(&module);
    if (status) {
      LOG(ERROR) << "Failed to load Bluetooth library";
      return false;
    }

    // Open the Bluetooth adapter.
    hw_device_t* device;
    status = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
    if (status) {
      LOG(ERROR) << "Failed to open the Bluetooth module";
      return false;
    }

    hal_adapter_ = reinterpret_cast<bluetooth_device_t*>(device);
    hal_iface_ = hal_adapter_->get_bluetooth_interface();

    // Initialize the Bluetooth interface. Set up the adapter (Bluetooth DM) API
    // callbacks.
    status = hal_iface_->init(&bt_callbacks);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to initialize Bluetooth stack";
      return false;
    }

    status = hal_iface_->set_os_callouts(&bt_os_callouts);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to set up Bluetooth OS callouts";
      return false;
    }

    return true;
  }

  base::ObserverList<Observer>* observers() { return &observers_; }

 private:
  // List of observers that are interested in notifications from us. We're not
  // using a base::ObserverListThreadSafe, which it posts observer events
  // automatically on the origin threads, as we want to avoid that overhead and
  // simply forward the events to the upper layer.
  base::ObserverList<Observer> observers_;

  // The HAL handle obtained from the shared library. We hold a weak reference
  // to this since the actual data resides in the shared Bluetooth library.
  const bt_interface_t* hal_iface_;

  // The HAL handle that represents the underlying Bluetooth adapter. We hold a
  // weak reference to this since the actual data resides in the shared
  // Bluetooth library.
  const bluetooth_device_t* hal_adapter_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothInterfaceImpl);
};

namespace {

// Helper for obtaining the observer list from the global instance. This
// function is NOT thread safe.
base::ObserverList<BluetoothInterface::Observer>* GetObservers() {
  CHECK(g_bluetooth_interface);
  return static_cast<BluetoothInterfaceImpl*>(
      g_bluetooth_interface)->observers();
}

}  // namespace

// static
bool BluetoothInterface::Initialize() {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(!g_bluetooth_interface);

  std::unique_ptr<BluetoothInterfaceImpl> impl(new BluetoothInterfaceImpl());
  if (!impl->Initialize()) {
    LOG(ERROR) << "Failed to initialize BluetoothInterface";
    return false;
  }

  g_bluetooth_interface = impl.release();

  return true;
}

// static
void BluetoothInterface::CleanUp() {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(g_bluetooth_interface);

  delete g_bluetooth_interface;
  g_bluetooth_interface = nullptr;
}

// static
BluetoothInterface* BluetoothInterface::Get() {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(g_bluetooth_interface);
  return g_bluetooth_interface;
}

// static
void BluetoothInterface::InitializeForTesting(
    BluetoothInterface* test_instance) {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(test_instance);
  CHECK(!g_bluetooth_interface);

  g_bluetooth_interface = test_instance;
}

}  // namespace hal
}  // namespace bluetooth
