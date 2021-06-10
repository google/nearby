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

#include <codecvt>
#include <locale>
#include <string>
#include <winstring.h>
#include "platform/impl/windows/bluetooth_classic_device.h"

using namespace winrt::Windows::Devices::Enumeration;

namespace location {
namespace nearby {
namespace windows {

BluetoothDevice::~BluetoothDevice(){}

BluetoothDevice::BluetoothDevice(
    winrt::hstring id, winrt::hstring name,
    winrt::Windows::Devices::Enumeration::DeviceInformationKind kind,
    winrt::Windows::Foundation::Collections::IMapView<
        winrt::hstring, winrt::Windows::Foundation::IInspectable>
        properties) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

  name_ = converter.to_bytes(name.c_str());
  id_ = converter.to_bytes(id.c_str());
}
    // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
std::string BluetoothDevice::GetName() const { return name_; }

// Returns BT MAC address assigned to this device.
// TODO(b/184975123): replace with real implementation.
std::string BluetoothDevice::GetMacAddress() const { return "Un-implemented"; }

}  // namespace windows
}  // namespace nearby
}  // namespace location