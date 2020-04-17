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

#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter {
 public:
  virtual ~BluetoothAdapter() {}

  // Eligible statuses of the BluetoothAdapter.
  struct Status {
    enum Value {
      DISABLED,
      ENABLED,
    };
  };

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  virtual bool setStatus(Status::Value status) = 0;
  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::ENABLED.
  virtual bool isEnabled() = 0;

  // Scan modes of a BluetoothAdapter, as described at
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode().
  struct ScanMode {
    enum Value {
      UNKNOWN,
      CONNECTABLE_DISCOVERABLE,
    };
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::UNKNOWN on error.
  virtual ScanMode::Value getScanMode() = 0;
  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  virtual bool setScanMode(ScanMode::Value scan_mode) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  //
  // Returns a null Ptr<string> on error.
  virtual Ptr<std::string> getName() = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  virtual bool setName(const std::string& name) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_BLUETOOTH_ADAPTER_H_
