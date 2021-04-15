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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_

#include <string>

#include "platform/api/bluetooth_adapter.h"

namespace location {
namespace nearby {
namespace windows {

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BluetoothAdapter() override = default;

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  // TODO(b/184975123): replace with real implementation.
  bool SetStatus(Status status) override { return false; }
  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  // TODO(b/184975123): replace with real implementation.
  bool IsEnabled() const override { return false; }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::kUnknown on error.
  // TODO(b/184975123): replace with real implementation.
  ScanMode GetScanMode() const override { return ScanMode::kUnknown; }
  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  // TODO(b/184975123): replace with real implementation.
  bool SetScanMode(ScanMode scan_mode) override { return false; }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  // TODO(b/184975123): replace with real implementation.
  std::string GetName() const override { return "Un-implemented"; }
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  // TODO(b/184975123): replace with real implementation.
  bool SetName(absl::string_view name) override { return false; }

  // Returns BT MAC address assigned to this adapter.
  // TODO(b/184975123): replace with real implementation.
  std::string GetMacAddress() const override { return "Un-implemented"; }
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_
