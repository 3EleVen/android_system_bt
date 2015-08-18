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

#include "service/ipc/binder/bluetooth_binder_server.h"

#include <base/logging.h>

#include "service/adapter.h"

namespace ipc {

BluetoothBinderServer::BluetoothBinderServer(bluetooth::Adapter* adapter)
    : adapter_(adapter) {
  CHECK(adapter_);
}

BluetoothBinderServer::~BluetoothBinderServer() {
}

// binder::BnBluetooth overrides:
bool BluetoothBinderServer::IsEnabled() {
  VLOG(2) << __func__;
  return adapter_->IsEnabled();
}

int BluetoothBinderServer::GetState() {
  VLOG(2) << __func__;
  return adapter_->GetState();
}

bool BluetoothBinderServer::Enable() {
  VLOG(2) << __func__;
  return adapter_->Enable();
}

bool BluetoothBinderServer::EnableNoAutoConnect() {
  VLOG(2) << __func__;
  // TODO(armansito): Implement.
  return false;
}

bool BluetoothBinderServer::Disable() {
  VLOG(2) << __func__;
  return adapter_->Disable();
}

std::string BluetoothBinderServer::GetAddress() {
  return adapter_->GetAddress();
}

std::vector<bluetooth::UUID> BluetoothBinderServer::GetUUIDs() {
  // TODO(armansito): Implement.
  return std::vector<bluetooth::UUID>();
}

bool BluetoothBinderServer::SetName(const std::string& name) {
  return adapter_->SetName(name);
}

std::string BluetoothBinderServer::GetName() {
  return adapter_->GetName();
}

}  // namespace ipc
