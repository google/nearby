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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_BLUETOOTH_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_BLUETOOTH_ADAPTER_H_

#include <stdint.h>

#include <array>
#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/base/bluetooth_address.h"
#include "internal/base/observer_list.h"
#include "sharing/internal/api/bluetooth_adapter.h"

namespace nearby {

class FakeBluetoothAdapter : public sharing::api::BluetoothAdapter {
 public:
  FakeBluetoothAdapter() {
    num_present_received_ = 0;
    num_powered_received_ = 0;
  }

  ~FakeBluetoothAdapter() override = default;

  bool IsPresent() const override { return is_present_; }

  bool IsPowered() const override {
    // If the bluetooth adapter is not present, return false for power status.
    if (!is_present_) {
      return false;
    }

    return is_powered_;
  }

  bool IsLowEnergySupported() const override {
    return is_low_energy_supported_;
  }

  bool IsScanOffloadSupported() const override {
    return is_scan_offload_supported_;
  }

  bool IsAdvertisementOffloadSupported() const override {
    return is_advertisement_offload_supported_;
  }

  bool IsExtendedAdvertisingSupported() const override {
    return is_extended_advertising_supported_;
  }

  bool IsPeripheralRoleSupported() const override {
    return is_peripheral_role_supported_;
  }

  sharing::api::BluetoothAdapter::PermissionStatus GetOsPermissionStatus()
      const override {
    return sharing::api::BluetoothAdapter::PermissionStatus::kAllowed;
  }

  void SetPowered(bool powered, std::function<void()> success_callback,
                  std::function<void()> error_callback) override {
    success_callback();
  }

  std::optional<std::string> GetAdapterId() const override { return "nearby"; }

  std::optional<std::array<uint8_t, 6>> GetAddress() const override {
    std::array<uint8_t, 6> output;
    if (mac_address_.has_value() && device::ParseBluetoothAddress(
            mac_address_.value(),
            absl::MakeSpan(output.data(), output.size()))) {
      return output;
    }
    return {};
  }

  void SetAddress(std::optional<absl::string_view> bluetooth_address) {
    mac_address_ = std::nullopt;
    if (bluetooth_address.has_value()) {
      mac_address_ = std::make_optional(std::string(bluetooth_address.value()));
    }
  }

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }
  bool HasObserver(Observer* observer) override {
    return observer_list_.HasObserver(observer);
  }

  // Mock OS bluetooth adapter presence state changed events
  void ReceivedAdapterPresentChangedFromOs(bool present) {
    num_present_received_ += 1;

    bool was_present = IsPresent();
    bool is_present = present;

    // Only trigger when state of presence changes values
    if (was_present != is_present) {
      is_present_ = is_present;
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->AdapterPresentChanged(this, is_present_);
        }
      }
    }
  }

  // Mock OS bluetooth adapter powered state changed events
  void ReceivedAdapterPoweredChangedFromOs(bool powered) {
    num_powered_received_ += 1;

    bool was_powered = IsPowered();
    bool is_powered = powered;

    // Only trigger when state of power changes values
    if (was_powered != is_powered) {
      is_powered_ = is_powered;
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->AdapterPoweredChanged(this, is_powered_);
        }
      }
    }
  }

  // Mock the behavior where the device does not support BLE
  void SetLowEnergySupported(bool is_supported) {
    is_low_energy_supported_ = is_supported;
  }

  // Mock the behavior where the device/OS not supports BLE scan offload
  void SetScanOffloadSupported(bool is_supported) {
    is_scan_offload_supported_ = is_supported;
  }

  // Mock the behavior where the device does not support offloaded advertisement
  void SetAdvertisementOffloadSupported(bool is_supported) {
    is_advertisement_offload_supported_ = is_supported;
  }

  // Mock the behavior where the device does not support extended advertising
  void SetExtendedAdvertisingSupported(bool is_supported) {
    is_extended_advertising_supported_ = is_supported;
  }

  // Mock the behavior where the device does not support BLE Peripheral Role
  void SetPeripheralRoleSupported(bool is_supported) {
    is_peripheral_role_supported_ = is_supported;
  }

  int GetNumPresentReceivedFromOS() { return num_present_received_; }
  int GetNumPoweredReceivedFromOS() { return num_powered_received_; }

 private:
  ObserverList<sharing::api::BluetoothAdapter::Observer> observer_list_;
  std::optional<std::string> mac_address_;
  bool is_present_ = true;
  bool is_powered_ = true;
  bool is_low_energy_supported_ = true;
  bool is_scan_offload_supported_ = true;
  bool is_advertisement_offload_supported_ = true;
  bool is_extended_advertising_supported_ = true;
  bool is_peripheral_role_supported_ = true;
  int num_present_received_;
  int num_powered_received_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_BLUETOOTH_ADAPTER_H_
