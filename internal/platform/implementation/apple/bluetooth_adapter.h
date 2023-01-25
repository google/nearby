// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_BLUETOOTH_ADAPTER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_BLUETOOTH_ADAPTER_H_

#include <string>

#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleCentral.h"

namespace nearby {
namespace apple {

class BluetoothAdapter;

// Concrete BlePeripheral implementation.
class BlePeripheral : public api::ble_v2::BlePeripheral {
 public:
  std::string GetAddress() const override;

  std::string GetPeripheralId() const { return peripheral_id_; }

  void SetPeripheralId(const std::string& peripheral_id) {
    peripheral_id_ = peripheral_id;
  }

  void SetConnectionRequester(GNCMBleConnectionRequester connection_requester) {
    connection_requester_ = connection_requester;
  }

  GNCMBleConnectionRequester GetConnectionRequester() {
    return connection_requester_;
  }

 private:
  // Only BluetoothAdapter may instantiate BlePeripheral.
  friend class BluetoothAdapter;

  explicit BlePeripheral(BluetoothAdapter* adapter) : adapter_(*adapter) {}

  BluetoothAdapter& adapter_;
  std::string peripheral_id_;
  GNCMBleConnectionRequester connection_requester_;
};

// Concrete BluetoothAdapter implementation.
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  using Status = api::BluetoothAdapter::Status;
  using ScanMode = api::BluetoothAdapter::ScanMode;

  ~BluetoothAdapter() override { SetStatus(Status::kDisabled); }

  bool SetStatus(Status status) override {
    enabled_ = status == Status::kEnabled;
    return true;
  }
  bool IsEnabled() const override { return enabled_; }
  ScanMode GetScanMode() const override { return mode_; }
  bool SetScanMode(ScanMode mode) override { return false; }
  std::string GetName() const override { return name_; }
  bool SetName(absl::string_view name) {
    return SetName(name, /* persist= */ true);
  }
  bool SetName(absl::string_view name, bool persist) override {
    name_ = std::string(name);
    return true;
  }
  std::string GetMacAddress() const override { return mac_address_; }
  void SetMacAddress(absl::string_view mac_address) {
    mac_address_ = std::string(mac_address);
  }

  BlePeripheral& GetPeripheral() { return peripheral_; }

 private:
  BlePeripheral peripheral_{this};
  ScanMode mode_ = ScanMode::kNone;
  std::string name_;
  std::string mac_address_;
  bool enabled_ = true;
};

}  // namespace apple
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_BLUETOOTH_ADAPTER_H_
