// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/adapter_client.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

bool BluetoothAdapter::SetStatus(Status status) {
  try {
    bool val = status == api::BluetoothAdapter::Status::kEnabled;
    bluez_adapter_->Powered(val);
    return true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_SET_ERROR(bluez_adapter_, "Powered", e);
    return false;
  }
}

bool BluetoothAdapter::IsEnabled() const {
  try {
    return bluez_adapter_->Powered();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Powered", e);
    return false;
  }
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  bool powered = IsEnabled();
  if (!powered) {
    return ScanMode::kNone;
  }

  try {
    bool discoverable = bluez_adapter_->Discoverable();
    return discoverable ? ScanMode::kConnectableDiscoverable
                        : ScanMode::kConnectable;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Discoverable", e);
    return ScanMode::kUnknown;
  }
}

bool BluetoothAdapter::SetScanMode(ScanMode scan_mode) {
  switch (scan_mode) {
    case ScanMode::kConnectable:
      return SetStatus(Status::kEnabled);
    case ScanMode::kConnectableDiscoverable: {
      if (!SetStatus(Status::kEnabled)) {
        return false;
      }

      try {
        bluez_adapter_->Discoverable(true);
      } catch (const sdbus::Error &e) {
        DBUS_LOG_PROPERTY_SET_ERROR(bluez_adapter_, "Discoverable", e);
        return false;
      }

      return true;
    }
    case ScanMode::kNone:
      return SetStatus(Status::kDisabled);
    default:
      return false;
  }
}

std::string BluetoothAdapter::GetName() const {
  try {
    return bluez_adapter_->Alias();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Alias", e);
    return {};
  }
}

bool BluetoothAdapter::SetName(absl::string_view name, bool  /*persist*/) {
  return SetName(name);
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  try {
    bluez_adapter_->Alias(std::string(name));
    return true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_SET_ERROR(bluez_adapter_, "Alias", e);
    return false;
  }
}

std::string BluetoothAdapter::GetMacAddress() const {
  try {
    return bluez_adapter_->Address();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Address", e);
    return {};
  }
}

}  // namespace linux
}  // namespace nearby
