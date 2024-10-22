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

#include <guiddef.h>

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {

// Represents an asynchronous operation, which returns a result upon completion.
// https://docs.microsoft.com/en-us/uwp/api/windows.foundation.iasyncoperation-1?view=winrt-20348
using winrt::Windows::Foundation::IAsyncOperation;

// The result of a Rfcomm device service request.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommdeviceservicesresult?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceServicesResult;

// Represents an instance of a service on a Bluetooth basic rate device.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommdeviceservice?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService;

// Represents an RFCOMM service ID.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommserviceid?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId;

// Indicates whether applicable Bluetooth API methods should operate on values
// cached in the system, or whether they should retrieve those values from the
// Bluetooth device.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothcachemode?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::BluetoothCacheMode;

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice : public api::BluetoothDevice {
 public:
  explicit BluetoothDevice(absl::string_view mac_address);
  explicit BluetoothDevice(
      const winrt::Windows::Devices::Bluetooth::BluetoothDevice&
          bluetoothDevice);

  ~BluetoothDevice() override;
  BluetoothDevice(BluetoothDevice&&) = default;
  BluetoothDevice& operator=(BluetoothDevice&&) = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  std::string GetName() const override { return name_; }

  // Returns BT MAC address assigned to this device.
  std::string GetMacAddress() const override;

  std::string GetId() { return id_; }

  // Update device name
  void SetName(std::string name) { name_ = name; }

  RfcommDeviceService GetRfcommServiceForIdAsync(
      winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId serviceId);

 private:
  RfcommDeviceService GetRfcommServiceForIdWithRetryAsync(
      winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId serviceId);

  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothdevice?view=winrt-20348
  winrt::Windows::Devices::Bluetooth::BluetoothDevice windows_bluetooth_device_;

  // Once the device is lost, we can no longer access it's id.
  std::string id_;

  // Keep the current device name, it can be updated when device update.
  std::string name_;

  // Once the device is lost, we can no longer access it's mac address.
  std::string mac_address_;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_DEVICE_H_
