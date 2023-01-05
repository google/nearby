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

#ifndef PLATFORM_API_BLUETOOTH_ADAPTER_H_
#define PLATFORM_API_BLUETOOTH_ADAPTER_H_

#include <string>

#include "absl/strings/string_view.h"

namespace nearby {
namespace api {

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter {
 public:
  virtual ~BluetoothAdapter() = default;

  // Eligible statuses of the BluetoothAdapter.
  enum class Status {
    kDisabled,
    kEnabled,
  };

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  virtual bool SetStatus(Status status) = 0;
  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  virtual bool IsEnabled() const = 0;

  // Scan modes of a BluetoothAdapter, as described at
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode().
  enum class ScanMode {
    kUnknown,
    kNone,
    kConnectable,
    kConnectableDiscoverable,
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::kUnknown on error.
  virtual ScanMode GetScanMode() const = 0;
  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  virtual bool SetScanMode(ScanMode scan_mode) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  virtual std::string GetName() const = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  virtual bool SetName(absl::string_view name) = 0;
  virtual bool SetName(absl::string_view name, bool persist) = 0;

  // Returns BT MAC address assigned to this adapter.
  virtual std::string GetMacAddress() const = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_BLUETOOTH_ADAPTER_H_
