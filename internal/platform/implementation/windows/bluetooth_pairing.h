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

#include <guiddef.h>

#include <string>

#include "winrt/Windows.Devices.Enumeration.h"

namespace nearby {
namespace windows {

class BluetoothPairing {
 public:
  explicit BluetoothPairing(
      ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing&
          custom_pairing);
  BluetoothPairing(const BluetoothPairing&) = default;
  BluetoothPairing& operator=(const BluetoothPairing&) = default;

  ~BluetoothPairing();

  // Initiates the pairing procedure.
  void StartPairing();

 private:
  void OnPairingRequested(
      ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing
          custom_pairing,
      ::winrt::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs
          pairing_requested);

  void OnPair(::winrt::Windows::Devices::Enumeration::DevicePairingResult&
                  pairing_result);

  // WinRT objects
  ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing
      custom_pairing_;
  ::winrt::event_token pairing_requested_token_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLUETOOTH_PAIRING_H_
