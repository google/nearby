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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_MANAGER_MEDIUMS_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_MANAGER_MEDIUMS_MANAGER_IMPL_H_

#include <guiddef.h>

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/windows/mediums_manager/mediums_manager.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"

namespace nearby {
namespace windows {

class MediumsManager : public api::MediumsManager {
 public:
  ~MediumsManager() override = default;

  bool StartBleAdvertising(
      absl::string_view service_id, const ByteArray& advertisement_bytes,
      absl::string_view fast_advertisement_service_uuid) override;

  bool StopBleAdvertising(absl::string_view service_id) override;

 private:
  void PublisherHandler(
      ::winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisher publisher,
      ::winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatusChangedEventArgs args);

  ::winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisher publisher_ = nullptr;

  ::winrt::event_token publisher_token_;

  bool is_publisher_started_ = false;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_MANAGER_MEDIUMS_MANAGER_IMPL_H_
