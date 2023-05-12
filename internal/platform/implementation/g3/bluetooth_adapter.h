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

#ifndef PLATFORM_IMPL_G3_BLUETOOTH_ADAPTER_H_
#define PLATFORM_IMPL_G3_BLUETOOTH_ADAPTER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/g3/single_thread_executor.h"

namespace nearby {
namespace g3 {

// BluetoothDevice and BluetoothAdapter have a mutual dependency.
class BluetoothAdapter;

// Opaque wrapper over a Ble peripheral. Must contain enough data about a
// particular Ble device to connect to its GATT server.
class BlePeripheral : public api::BlePeripheral {
 public:
  ~BlePeripheral() override = default;

  std::string GetName() const override;
  ByteArray GetAdvertisementBytes(const std::string& service_id) const override;
  void SetAdvertisementBytes(const std::string& service_id,
                             const ByteArray& advertisement_bytes);
  BluetoothAdapter& GetAdapter() { return adapter_; }

 private:
  // Only BluetoothAdapter may instantiate BlePeripheral.
  friend class BluetoothAdapter;

  explicit BlePeripheral(BluetoothAdapter* adapter);

  BluetoothAdapter& adapter_;
  ByteArray advertisement_bytes_;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice : public api::BluetoothDevice {
 public:
  ~BluetoothDevice() override = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  std::string GetName() const override;
  std::string GetMacAddress() const override;
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

  BluetoothAdapter();
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
  bool SetName(absl::string_view) override ABSL_LOCKS_EXCLUDED(mutex_);
  bool SetName(absl::string_view name, bool persist) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns BT MAC address assigned to this adapter.
  std::string GetMacAddress() const override { return mac_address_; }

  BluetoothDevice& GetDevice() { return device_; }

  void SetBluetoothClassicMedium(api::BluetoothClassicMedium* medium);
  api::BluetoothClassicMedium* GetBluetoothClassicMedium() {
    return bluetooth_classic_medium_;
  }

  BlePeripheral& GetPeripheral() { return peripheral_; }

  void SetBleMedium(api::BleMedium* medium);
  api::BleMedium* GetBleMedium() { return ble_medium_; }

  void SetBleV2Medium(api::ble_v2::BleMedium* medium);
  api::ble_v2::BleMedium* GetBleV2Medium() { return ble_v2_medium_; }

  void SetMacAddress(absl::string_view mac_address) {
    mac_address_ = std::string(mac_address);
  }

  std::uint64_t GetUniqueId() { return unique_id_; }

 private:
  mutable absl::Mutex mutex_;
  BluetoothDevice device_{this};
  BlePeripheral peripheral_{this};
  api::BluetoothClassicMedium* bluetooth_classic_medium_ = nullptr;
  api::BleMedium* ble_medium_ = nullptr;
  api::ble_v2::BleMedium* ble_v2_medium_ = nullptr;
  std::string mac_address_;
  ScanMode mode_ ABSL_GUARDED_BY(mutex_) = ScanMode::kNone;
  std::string name_ ABSL_GUARDED_BY(mutex_) = "unknown G3 BT device";
  bool enabled_ ABSL_GUARDED_BY(mutex_) = true;
  std::uint64_t unique_id_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_BLUETOOTH_ADAPTER_H_
