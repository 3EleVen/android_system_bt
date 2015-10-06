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

#include "service/hal/bluetooth_gatt_interface.h"

#include <mutex>

#include <base/logging.h>
#include <base/observer_list.h>

#include "service/hal/bluetooth_interface.h"

using std::lock_guard;
using std::mutex;

namespace bluetooth {
namespace hal {

namespace {

// The global BluetoothGattInterface instance.
BluetoothGattInterface* g_interface = nullptr;

// Mutex used by callbacks to access |g_interface|.
mutex g_instance_lock;

// Helper for obtaining the observer lists. This is forward declared here
// and defined below since it depends on BluetoothInterfaceImpl.
base::ObserverList<BluetoothGattInterface::ClientObserver>*
    GetClientObservers();
base::ObserverList<BluetoothGattInterface::ServerObserver>*
    GetServerObservers();

#define FOR_EACH_CLIENT_OBSERVER(func) \
  FOR_EACH_OBSERVER(BluetoothGattInterface::ClientObserver, \
                    *GetClientObservers(), func)

#define FOR_EACH_SERVER_OBSERVER(func) \
  FOR_EACH_OBSERVER(BluetoothGattInterface::ServerObserver, \
                    *GetServerObservers(), func)

#define VERIFY_INTERFACE_OR_RETURN() \
  if (!g_interface) { \
    LOG(WARNING) << "Callback received after |g_interface| is NULL"; \
    return; \
  }

void RegisterClientCallback(int status, int client_if, bt_uuid_t* app_uuid) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(app_uuid);

  FOR_EACH_CLIENT_OBSERVER(
      RegisterClientCallback(g_interface, status, client_if, *app_uuid));
}

void MultiAdvEnableCallback(int client_if, int status) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvEnableCallback(g_interface, client_if, status));
}

void MultiAdvUpdateCallback(int client_if, int status) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvUpdateCallback(g_interface, client_if, status));
}

void MultiAdvDataCallback(int client_if, int status) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvDataCallback(g_interface, client_if, status));
}

void MultiAdvDisableCallback(int client_if, int status) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvDisableCallback(g_interface, client_if, status));
}

void RegisterServerCallback(int status, int server_if, bt_uuid_t* app_uuid) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(app_uuid);

  FOR_EACH_SERVER_OBSERVER(
      RegisterServerCallback(g_interface, status, server_if, *app_uuid));
}

void ConnectionCallback(int conn_id, int server_if, int connected,
                        bt_bdaddr_t* bda) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " server_if: " << server_if << " connected: " << connected;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(
      ConnectionCallback(g_interface, conn_id, server_if, connected, *bda));
}

void ServiceAddedCallback(
    int status,
    int server_if,
    btgatt_srvc_id_t* srvc_id,
    int srvc_handle) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " handle: " << srvc_handle;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(srvc_id);

  FOR_EACH_SERVER_OBSERVER(ServiceAddedCallback(
      g_interface, status, server_if, *srvc_id, srvc_handle));
}

void CharacteristicAddedCallback(
    int status, int server_if,
    bt_uuid_t* uuid,
    int srvc_handle,
    int char_handle) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " srvc_handle: " << srvc_handle << " char_handle: " << char_handle;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(uuid);

  FOR_EACH_SERVER_OBSERVER(CharacteristicAddedCallback(
      g_interface, status, server_if, *uuid, srvc_handle, char_handle));
}

void DescriptorAddedCallback(
    int status, int server_if,
    bt_uuid_t* uuid,
    int srvc_handle,
    int desc_handle) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " srvc_handle: " << srvc_handle << " desc_handle: " << desc_handle;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(uuid);

  FOR_EACH_SERVER_OBSERVER(DescriptorAddedCallback(
      g_interface, status, server_if, *uuid, srvc_handle, desc_handle));
}

void ServiceStartedCallback(int status, int server_if, int srvc_handle) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " handle: " << srvc_handle;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(ServiceStartedCallback(
      g_interface, status, server_if, srvc_handle));
}

void ServiceStoppedCallback(int status, int server_if, int srvc_handle) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " handle: " << srvc_handle;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(ServiceStoppedCallback(
      g_interface, status, server_if, srvc_handle));
}

void RequestReadCallback(int conn_id, int trans_id, bt_bdaddr_t* bda,
                         int attr_handle, int offset, bool is_long) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " attr_handle: " << attr_handle << " offset: " << offset
          << " is_long: " << is_long;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(RequestReadCallback(
      g_interface, conn_id, trans_id, *bda, attr_handle, offset, is_long));
}

void RequestWriteCallback(int conn_id, int trans_id,
                          bt_bdaddr_t* bda,
                          int attr_handle, int offset, int length,
                          bool need_rsp, bool is_prep, uint8_t* value) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " attr_handle: " << attr_handle << " offset: " << offset
          << " length: " << length << " need_rsp: " << need_rsp
          << " is_prep: " << is_prep;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(RequestWriteCallback(
      g_interface, conn_id, trans_id, *bda, attr_handle, offset, length,
      need_rsp, is_prep, value));
}

void RequestExecWriteCallback(int conn_id, int trans_id,
                              bt_bdaddr_t* bda, int exec_write) {
  lock_guard<mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " exec_write: " << exec_write;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(RequestExecWriteCallback(
      g_interface, conn_id, trans_id, *bda, exec_write));
}

// The HAL Bluetooth GATT client interface callbacks. These signal a mixture of
// GATT client-role and GAP events.
const btgatt_client_callbacks_t gatt_client_callbacks = {
    RegisterClientCallback,
    nullptr,  // scan_result_cb
    nullptr,  // open_cb
    nullptr,  // close_cb
    nullptr,  // search_complete_cb
    nullptr,  // search_result_cb
    nullptr,  // get_characteristic_cb
    nullptr,  // get_descriptor_cb
    nullptr,  // get_included_service_cb
    nullptr,  // register_for_notification_cb
    nullptr,  // notify_cb
    nullptr,  // read_characteristic_cb
    nullptr,  // write_characteristic_cb
    nullptr,  // read_descriptor_cb
    nullptr,  // write_descriptor_cb
    nullptr,  // execute_write_cb
    nullptr,  // read_remote_rssi_cb
    nullptr,  // listen_cb
    nullptr,  // configure_mtu_cb
    nullptr,  // scan_filter_cfg_cb
    nullptr,  // scan_filter_param_cb
    nullptr,  // scan_filter_status_cb
    MultiAdvEnableCallback,
    MultiAdvUpdateCallback,
    MultiAdvDataCallback,
    MultiAdvDisableCallback,
    nullptr,  // congestion_cb
    nullptr,  // batchscan_cfg_storage_cb
    nullptr,  // batchscan_enb_disable_cb
    nullptr,  // batchscan_reports_cb
    nullptr,  // batchscan_threshold_cb
    nullptr,  // track_adv_event_cb
    nullptr,  // scan_parameter_setup_completed_cb
};

const btgatt_server_callbacks_t gatt_server_callbacks = {
    RegisterServerCallback,
    ConnectionCallback,
    ServiceAddedCallback,
    nullptr,  // included_service_added_cb,
    CharacteristicAddedCallback,
    DescriptorAddedCallback,
    ServiceStartedCallback,
    ServiceStoppedCallback,
    nullptr,  // service_deleted_cb,
    RequestReadCallback,
    RequestWriteCallback,
    RequestExecWriteCallback,
    nullptr,  // response_confirmation_cb,
    nullptr,  // indication_sent_cb
    nullptr,  // congestion_cb
    nullptr,  // mtu_changed_cb
};

const btgatt_callbacks_t gatt_callbacks = {
  sizeof(btgatt_callbacks_t),
  &gatt_client_callbacks,
  &gatt_server_callbacks
};

}  // namespace

// BluetoothGattInterface implementation for production.
class BluetoothGattInterfaceImpl : public BluetoothGattInterface {
 public:
  BluetoothGattInterfaceImpl() : hal_iface_(nullptr) {
  }

  ~BluetoothGattInterfaceImpl() override {
    if (hal_iface_)
        hal_iface_->cleanup();
  }

  // BluetoothGattInterface overrides:
  void AddClientObserver(ClientObserver* observer) override {
    lock_guard<mutex> lock(g_instance_lock);
    AddClientObserverUnsafe(observer);
  }

  void RemoveClientObserver(ClientObserver* observer) override {
    lock_guard<mutex> lock(g_instance_lock);
    RemoveClientObserverUnsafe(observer);
  }

  void AddClientObserverUnsafe(ClientObserver* observer) override {
    client_observers_.AddObserver(observer);
  }

  void RemoveClientObserverUnsafe(ClientObserver* observer) override {
    client_observers_.RemoveObserver(observer);
  }

  void AddServerObserver(ServerObserver* observer) override {
    lock_guard<mutex> lock(g_instance_lock);
    AddServerObserverUnsafe(observer);
  }

  void RemoveServerObserver(ServerObserver* observer) override {
    lock_guard<mutex> lock(g_instance_lock);
    RemoveServerObserverUnsafe(observer);
  }

  void AddServerObserverUnsafe(ServerObserver* observer) override {
    server_observers_.AddObserver(observer);
  }

  void RemoveServerObserverUnsafe(ServerObserver* observer) override {
    server_observers_.RemoveObserver(observer);
  }

  const btgatt_client_interface_t* GetClientHALInterface() const override {
    return hal_iface_->client;
  }

  const btgatt_server_interface_t* GetServerHALInterface() const override {
    return hal_iface_->server;
  }

  // Initialize the interface.
  bool Initialize() {
    const bt_interface_t* bt_iface =
        BluetoothInterface::Get()->GetHALInterface();
    CHECK(bt_iface);

    const btgatt_interface_t* gatt_iface =
        reinterpret_cast<const btgatt_interface_t*>(
            bt_iface->get_profile_interface(BT_PROFILE_GATT_ID));
    if (!gatt_iface) {
      LOG(ERROR) << "Failed to obtain HAL GATT interface handle";
      return false;
    }

    bt_status_t status = gatt_iface->init(&gatt_callbacks);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to initialize HAL GATT interface";
      return false;
    }

    hal_iface_ = gatt_iface;

    return true;
  }

  base::ObserverList<ClientObserver>* client_observers() {
    return &client_observers_;
  }

  base::ObserverList<ServerObserver>* server_observers() {
    return &server_observers_;
  }

 private:
  // List of observers that are interested in notifications from us.
  // We're not using a base::ObserverListThreadSafe, which it posts observer
  // events automatically on the origin threads, as we want to avoid that
  // overhead and simply forward the events to the upper layer.
  base::ObserverList<ClientObserver> client_observers_;
  base::ObserverList<ServerObserver> server_observers_;

  // The HAL handle obtained from the shared library. We hold a weak reference
  // to this since the actual data resides in the shared Bluetooth library.
  const btgatt_interface_t* hal_iface_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattInterfaceImpl);
};

namespace {

base::ObserverList<BluetoothGattInterface::ClientObserver>*
GetClientObservers() {
  CHECK(g_interface);
  return static_cast<BluetoothGattInterfaceImpl*>(
      g_interface)->client_observers();
}

base::ObserverList<BluetoothGattInterface::ServerObserver>*
GetServerObservers() {
  CHECK(g_interface);
  return static_cast<BluetoothGattInterfaceImpl*>(
      g_interface)->server_observers();
}

}  // namespace

// Default observer implementations. These are provided so that the methods
// themselves are optional.
void BluetoothGattInterface::ClientObserver::RegisterClientCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */,
    const bt_uuid_t& /* app_uuid */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::MultiAdvEnableCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::MultiAdvUpdateCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::MultiAdvDataCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::MultiAdvDisableCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RegisterServerCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const bt_uuid_t& /* app_uuid */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ConnectionCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* server_if */,
    int /* connected */,
    const bt_bdaddr_t& /* bda */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ServiceAddedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const btgatt_srvc_id_t& /* srvc_id */,
    int /* srvc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::CharacteristicAddedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const bt_uuid_t& /* uuid */,
    int /* srvc_handle */,
    int /* char_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::DescriptorAddedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const bt_uuid_t& /* uuid */,
    int /* srvc_handle */,
    int /* desc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ServiceStartedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    int /* srvc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ServiceStoppedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    int /* srvc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RequestReadCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* trans_id */,
    const bt_bdaddr_t& /* bda */,
    int /* attr_handle */,
    int /* offset */,
    bool /* is_long */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RequestWriteCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* trans_id */,
    const bt_bdaddr_t& /* bda */,
    int /* attr_handle */,
    int /* offset */,
    int /* length */,
    bool /* need_rsp */,
    bool /* is_prep */,
    uint8_t* /* value */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RequestExecWriteCallback(
    BluetoothGattInterface* gatt_iface,
    int /* conn_id */,
    int /* trans_id */,
    const bt_bdaddr_t& /* bda */,
    int /* exec_write */) {
  // Do nothing.
}

// static
bool BluetoothGattInterface::Initialize() {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(!g_interface);

  std::unique_ptr<BluetoothGattInterfaceImpl> impl(
      new BluetoothGattInterfaceImpl());
  if (!impl->Initialize()) {
    LOG(ERROR) << "Failed to initialize BluetoothGattInterface";
    return false;
  }

  g_interface = impl.release();

  return true;
}

// static
void BluetoothGattInterface::CleanUp() {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(g_interface);

  delete g_interface;
  g_interface = nullptr;
}

// static
bool BluetoothGattInterface::IsInitialized() {
  lock_guard<mutex> lock(g_instance_lock);

  return g_interface != nullptr;
}

// static
BluetoothGattInterface* BluetoothGattInterface::Get() {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(g_interface);
  return g_interface;
}

// static
void BluetoothGattInterface::InitializeForTesting(
    BluetoothGattInterface* test_instance) {
  lock_guard<mutex> lock(g_instance_lock);
  CHECK(test_instance);
  CHECK(!g_interface);

  g_interface = test_instance;
}

}  // namespace hal
}  // namespace bluetooth
