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

#include <winstring.h>

#include <codecvt>
#include <locale>
#include <string>

#include "platform/impl/windows/generated/winrt/Windows.Devices.Bluetooth.h"

#include "platform/impl/windows/bluetooth_classic_device.h"

namespace location {
namespace nearby {
namespace windows {

BluetoothDevice::~BluetoothDevice() {}

BluetoothDevice::BluetoothDevice(
    const winrt::Windows::Devices::Bluetooth::BluetoothDevice &bluetoothDevice)
    : windows_bluetooth_device_(bluetoothDevice) {
  id_ = winrt::to_string(bluetoothDevice.DeviceId());
  mac_address_ = bluetoothDevice.BluetoothAddress();
}

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
std::string BluetoothDevice::GetName() const {
  return winrt::to_string(windows_bluetooth_device_.Name());
}

// Returns BT MAC address assigned to this device.
// TODO(b/184975123): replace with real implementation.
std::string BluetoothDevice::GetMacAddress() const { return "Un-implemented"; }

}  // namespace windows
}  // namespace nearby
}  // namespace location
