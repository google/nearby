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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_H_

#include <string>

#include "platform/api/bluetooth_classic.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"
#include "platform/base/output_stream.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/base.h"

namespace location {
namespace nearby {
namespace windows {

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice : public api::BluetoothDevice {
 public:
  explicit BluetoothDevice(
      winrt::hstring id, winrt::hstring name,
      winrt::Windows::Devices::Enumeration::DeviceInformationKind kind,
      winrt::Windows::Foundation::Collections::IMapView<
          winrt::hstring, winrt::Windows::Foundation::IInspectable>
          properties);

  ~BluetoothDevice() override;
  BluetoothDevice(BluetoothDevice&&) = default;
  BluetoothDevice& operator=(BluetoothDevice&&) = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  // TODO(b/184975123): replace with real implementation.
  std::string GetName() const override;

  // Returns BT MAC address assigned to this device.
  // TODO(b/184975123): replace with real implementation.
  std::string GetMacAddress() const override;

  std::string GetId() { return id_; }

 private:
  std::string name_;
  std::string id_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_H_
