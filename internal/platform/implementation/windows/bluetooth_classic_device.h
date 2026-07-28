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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_DEVICE_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_DEVICE_H_

#include <string>

#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/mac_address.h"

namespace nearby::windows {

class BluetoothDevice : public api::BluetoothDevice {
 public:
  // Create a BluetoothDevice from device info during scanning.
  // The WinRT bluetooth device is not created until connection.
  BluetoothDevice(absl::string_view device_id, absl::string_view device_name,
                  MacAddress mac_address)
      : windows_bluetooth_device_(nullptr),
        id_(device_id),
        name_(device_name),
        mac_address_(mac_address) {}
  // Create a BluetoothDevice from WinRT bluetooth device during connection.
  explicit BluetoothDevice(
      const winrt::Windows::Devices::Bluetooth::BluetoothDevice&
          bluetoothDevice);

  ~BluetoothDevice() override;
  BluetoothDevice(BluetoothDevice&&) = default;
  BluetoothDevice& operator=(BluetoothDevice&&) = default;

  std::string GetName() const override { return name_; }

  // Returns BT MAC address assigned to this device.
  MacAddress GetMacAddress() const override;

  std::string GetId() { return id_; }

  // Update device name
  void SetName(std::string name) { name_ = name; }

  winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService
  GetRfcommServiceForIdAsync(
      winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId serviceId);

 private:
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothdevice?view=winrt-20348
  winrt::Windows::Devices::Bluetooth::BluetoothDevice windows_bluetooth_device_;

  // Once the device is lost, we can no longer access it's id.
  std::string id_;

  // Keep the current device name, it can be updated when device update.
  std::string name_;

  // Once the device is lost, we can no longer access it's mac address.
  MacAddress mac_address_;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_DEVICE_H_
