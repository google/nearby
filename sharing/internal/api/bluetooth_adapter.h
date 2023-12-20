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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_BLUETOOTH_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_BLUETOOTH_ADAPTER_H_

#include <array>
#include <functional>
#include <optional>
#include <string>

namespace nearby {
namespace sharing {
namespace api {

class BluetoothAdapter {
 public:
  enum class PermissionStatus {
    kUndetermined = 0,
    kSystemDenied,
    kUserDenied,
    kAllowed
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the presence of the adapter |adapter| changes. When |present|
    // is true the adapter is now present, false means the adapter has been
    // removed from the system.
    virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                       bool present) {}

    // Called when the radio power state of the adapter |adapter| changes. When
    // |powered| is true the adapter radio is powered, false means the adapter
    // radio is off.
    virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                       bool powered) {}
  };

  virtual ~BluetoothAdapter() = default;

  // Indicates whether the adapter is actually present on the system. An adapter
  // is only considered present if the bluetooth mac address has been obtained.
  virtual bool IsPresent() const = 0;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const = 0;

  // Indicates whether the adapter supports BLE.
  virtual bool IsLowEnergySupported() const = 0;

  // Indicates whether the adapter supports BLE offloads to scan.
  virtual bool IsScanOffloadSupported() const = 0;

  // Indicates whether the adapter supports BLE advertisement offload.
  virtual bool IsAdvertisementOffloadSupported() const = 0;

  // Indicates whether the adapter supports BLE 5.0 Extended Advertising.
  virtual bool IsExtendedAdvertisingSupported() const = 0;

  // Indicates whether the adapter supports BLE Peripheral Role.
  virtual bool IsPeripheralRoleSupported() const = 0;

  // Returns the status of the browser's Bluetooth permission status.
  virtual PermissionStatus GetOsPermissionStatus() const = 0;

  // Requests a change to the adapter radio power. Setting |powered| to true
  // will turn on the radio and false will turn it off. On success,
  // |success_callback| will be called. On failure, |error_callback| will be
  // called.
  virtual void SetPowered(bool powered, std::function<void()> success_callback,
                          std::function<void()> error_callback) = 0;

  // The unique ID/name of this adapter.
  virtual std::optional<std::string> GetAdapterId() const = 0;

  // The mac address of this adapter.
  virtual std::optional<std::array<uint8_t, 6>> GetAddress() const = 0;

  // Adds and removes observers for events on this bluetooth adapter. If
  // monitoring multiple adapters, check the |adapter| parameter of observer
  // methods to determine which adapter is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;
};

}  // namespace api
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_BLUETOOTH_ADAPTER_H_
