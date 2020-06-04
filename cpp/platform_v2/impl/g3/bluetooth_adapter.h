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

#ifndef PLATFORM_V2_IMPL_G3_BLUETOOTH_ADAPTER_H_
#define PLATFORM_V2_IMPL_G3_BLUETOOTH_ADAPTER_H_

#include <string>

#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/impl/g3/single_thread_executor.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

// BluetoothDevice and BluetoothAdapter have a mutual dependency.
class BluetoothAdapter;

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice : public api::BluetoothDevice {
 public:
  ~BluetoothDevice() override = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  std::string GetName() const override;
  BluetoothAdapter& GetAdapter() { return adapter_; }

 private:
  // Only BluetoothAdapter may instantiate BluetoothDevice.
  friend class BluetoothAdapter;

  explicit BluetoothDevice(BluetoothAdapter* adapter);

  BluetoothAdapter& adapter_;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  using Status = api::BluetoothAdapter::Status;
  using ScanMode = api::BluetoothAdapter::ScanMode;

  explicit BluetoothAdapter() = default;
  ~BluetoothAdapter() override;

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  bool SetStatus(Status status) override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  bool IsEnabled() const override ABSL_LOCKS_EXCLUDED(mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::kUnknown on error.
  ScanMode GetScanMode() const override ABSL_LOCKS_EXCLUDED(mutex_);

  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  bool SetScanMode(ScanMode mode) override ABSL_LOCKS_EXCLUDED(mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  std::string GetName() const override ABSL_LOCKS_EXCLUDED(mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  bool SetName(absl::string_view name) override ABSL_LOCKS_EXCLUDED(mutex_);

  BluetoothDevice& GetDevice() { return device_; }

  void SetMedium(api::BluetoothClassicMedium* medium);
  api::BluetoothClassicMedium* GetMedium() { return medium_; }

 private:
  mutable absl::Mutex mutex_;
  BluetoothDevice device_{this};
  api::BluetoothClassicMedium* medium_ = nullptr;
  ScanMode mode_ ABSL_GUARDED_BY(mutex_) = ScanMode::kNone;
  std::string name_ ABSL_GUARDED_BY(mutex_) = "unknown G3 BT device";
  bool enabled_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_BLUETOOTH_ADAPTER_H_
