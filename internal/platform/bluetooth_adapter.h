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

#ifndef PLATFORM_PUBLIC_BLUETOOTH_ADAPTER_H_
#define PLATFORM_PUBLIC_BLUETOOTH_ADAPTER_H_

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE peripheral to connect to its GATT server.
class BlePeripheral final {
 public:
  BlePeripheral() = default;
  BlePeripheral(const BlePeripheral&) = default;
  BlePeripheral& operator=(const BlePeripheral&) = default;
  explicit BlePeripheral(api::BlePeripheral* peripheral) : impl_(peripheral) {}

  std::string GetName() const { return impl_->GetName(); }

  ByteArray GetAdvertisementBytes(const std::string& service_id) const {
    return impl_->GetAdvertisementBytes(service_id);
  }

  api::BlePeripheral& GetImpl() { return *impl_; }
  bool IsValid() const { return impl_ != nullptr; }

 private:
  api::BlePeripheral* impl_;
};

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE peripheral to connect to its GATT server.
class BleV2Peripheral final {
 public:
  BleV2Peripheral() = default;
  explicit BleV2Peripheral(api::ble_v2::BlePeripheral* peripheral)
      : impl_(peripheral) {}
  BleV2Peripheral(const BleV2Peripheral&) = default;
  BleV2Peripheral& operator=(const BleV2Peripheral&) = default;
  BleV2Peripheral(BleV2Peripheral&& other) {
    impl_ = other.impl_;
    id_ = std::move(other.id_);
    psm_ = other.psm_;

    other.impl_ = nullptr;
    other.psm_ = 0;
  }

  BleV2Peripheral& operator=(BleV2Peripheral&& other) {
    if (this != &other) {
      impl_ = other.impl_;
      id_ = std::move(other.id_);
      psm_ = other.psm_;

      other.impl_ = nullptr;
      other.psm_ = 0;
    }
    return *this;
  }

  std::string GetAddress() const { return impl_->GetAddress(); }

  ByteArray GetId() const { return id_; }
  void SetId(const ByteArray& id) { id_ = id; }

  int GetPsm() const { return psm_; }
  void SetPsm(int psm) { psm_ = psm; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  api::ble_v2::BlePeripheral& GetImpl() const { return *impl_; }
  bool IsValid() const { return impl_ != nullptr; }

 private:
  // Does not take ownership. It refers to a valid `api::ble_v2::BlePeripheral`
  // that outlives this object.
  api::ble_v2::BlePeripheral* impl_ = nullptr;

  // A unique identifier for this peripheral. It is the BLE advertisement bytes
  // it was found on.
  ByteArray id_ = {};

  // The psm (protocol service multiplexer) value is used for create data
  // connection on L2CAP socket. It only exists when remote device supports
  // L2CAP socket feature.
  int psm_ = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice final {
 public:
  BluetoothDevice() = default;
  BluetoothDevice(const BluetoothDevice&) = default;
  BluetoothDevice& operator=(const BluetoothDevice&) = default;
  explicit BluetoothDevice(api::BluetoothDevice* device) : impl_(device) {}
  ~BluetoothDevice() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  std::string GetName() const { return impl_->GetName(); }
  std::string GetMacAddress() const { return impl_->GetMacAddress(); }

  api::BluetoothDevice& GetImpl() { return *impl_; }
  bool IsValid() const { return impl_ != nullptr; }

 private:
  api::BluetoothDevice* impl_;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter final {
 public:
  using Status = api::BluetoothAdapter::Status;
  using ScanMode = api::BluetoothAdapter::ScanMode;

  BluetoothAdapter()
      : impl_(api::ImplementationPlatform::CreateBluetoothAdapter()) {}
  BluetoothAdapter(BluetoothAdapter&&) = default;
  BluetoothAdapter& operator=(BluetoothAdapter&&) = default;

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  bool SetStatus(Status status) { return impl_->SetStatus(status); }
  Status GetStatus() const {
    return IsEnabled() ? Status::kEnabled : Status::kDisabled;
  }

  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  bool IsEnabled() const { return impl_->IsEnabled(); }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::kUnknown on error.
  ScanMode GetScanMode() const { return impl_->GetScanMode(); }

  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  bool SetScanMode(ScanMode scan_mode) { return impl_->SetScanMode(scan_mode); }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  std::string GetName() const { return impl_->GetName(); }
  std::string GetMacAddress() const { return impl_->GetMacAddress(); }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  bool SetName(absl::string_view name) {
    // The name always persists resulting in not saving the current bluetooth
    // radio name.
    return impl_->SetName(name,
                          /* persist= */ true);
  }

  // If persist is set to true, we will not update the stored radio names.
  // If persist is set to false, we will update the stored radio names.
  bool SetName(absl::string_view name, bool persist) {
    return impl_->SetName(name, persist);
  }

  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while BluetoothAdapter object is
  // itself valid. It matches Core() object lifetime.
  api::BluetoothAdapter& GetImpl() { return *impl_; }

 private:
  std::unique_ptr<api::BluetoothAdapter> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_BLUETOOTH_ADAPTER_H_
