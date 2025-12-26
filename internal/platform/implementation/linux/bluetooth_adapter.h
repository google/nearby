// Copyright 2020 Google LLC
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

#ifndef PLATFORM_LINUX_IMPL_BLUETOOTH_ADAPTER_H_
#define PLATFORM_LINUX_IMPL_BLUETOOTH_ADAPTER_H_

#include <string>
#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/generated/bluez_adapter_client_glue.h"

constexpr uint8_t kAndroidDiscoverableBluetoothNameMaxLength = 37;  // bytes

namespace nearby {
namespace linux {

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter : public api::BluetoothAdapter, public sdbus::ProxyInterfaces<org::bluez::Adapter1_proxy> {
 public:
  BluetoothAdapter(sdbus::IConnection& system_bus,
                   const sdbus::ObjectPath& object_path): ProxyInterfaces(
                     system_bus,
                     "org.bluez.Adap", object_path )
  {
    registerProxy();
  };
  ~BluetoothAdapter() override
  {
    unregisterProxy();
  };

  //// Eligible statuses of the BluetoothAdapter.
  //enum class Status {
  //  kDisabled,
  //  kEnabled,
  //};

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  bool SetStatus(Status status) override;
  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  [[nodiscard]] bool IsEnabled() const override;

  // Scan modes of a BluetoothAdapter, as described at
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode().
  //enum class ScanMode {
  //  kUnknown,
  //  kNone,
  //  kConnectable,
  //  kConnectableDiscoverable,
  //};

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::kUnknown on error.
  [[nodiscard]] ScanMode GetScanMode() const override;
  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  bool SetScanMode(ScanMode scan_mode) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  [[nodiscard]] std::string GetName() const override;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  bool SetName(absl::string_view name) override;
  bool SetName(absl::string_view name, bool persist) override;

  // Returns BT MAC address assigned to this adapter.
  ABSL_DEPRECATED("Use GetAddress() instead.")
  [[nodiscard]] std::string GetMacAddress() const override;

  // Implementation for migration only.  Once subclasses implement this, the
  // above GetMacAddress() can be removed.
  [[nodiscard]] MacAddress GetAddress() const override {
    std::string mac_address = GetMacAddress();
    if (mac_address.empty()) {
      return {};
    }
    MacAddress address;
    MacAddress::FromString(mac_address, address);
    return address;
  }
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_LINUX_IMPL_BLUETOOTH_ADAPTER_H_
