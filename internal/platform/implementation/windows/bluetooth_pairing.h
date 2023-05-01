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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLUETOOTH_PAIRING_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLUETOOTH_PAIRING_H_

#include <windows.h>

#include <optional>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

class BluetoothPairing : public api::BluetoothPairing {
 public:
  explicit BluetoothPairing(
      ::winrt::Windows::Devices::Bluetooth::BluetoothDevice bluetooth_device,
      ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing
          custom_pairing);
  BluetoothPairing(const BluetoothPairing&) = default;
  BluetoothPairing& operator=(const BluetoothPairing&) = default;
  ~BluetoothPairing() override;

  bool InitiatePairing(api::BluetoothPairingCallback pairing_cb) override;
  bool FinishPairing(std::optional<absl::string_view> pin_code) override;
  bool CancelPairing() override;
  bool Unpair() override;
  bool IsPaired() override;

 private:
  void OnPairingRequested(
      ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing
          custom_pairing,
      ::winrt::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs
          pairing_requested);

  void OnPair(::winrt::Windows::Devices::Enumeration::DevicePairingResult&
                  pairing_result);

  // WinRT objects
  ::winrt::Windows::Devices::Bluetooth::BluetoothDevice bluetooth_device_;
  ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing
      custom_pairing_;

  ::winrt::event_token pairing_requested_token_;
  ::winrt::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs
      pairing_requested_ = nullptr;
  ::winrt::Windows::Foundation::Deferral pairing_deferral_ = nullptr;
  api::BluetoothPairingCallback pairing_callback_;

  // Boolean indicating whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  bool expecting_pin_code_ = false;
  bool was_cancelled_ = false;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLUETOOTH_PAIRING_H_
