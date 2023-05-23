// Copyright 2023 Google LLC
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

#include "fastpair/repository/fast_pair_device_repository.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace nearby {
namespace fastpair {

FastPairDevice* FastPairDeviceRepository::AddDevice(
    std::unique_ptr<FastPairDevice> device) {
  const auto& id = device->GetUniqueId();
  for (auto& item : devices_) {
    if (item->GetUniqueId() == id) {
      // Overwrite the existing object.
      *item = std::move(*device);
      return item.get();
    }
  }
  FastPairDevice* ptr = device.get();
  devices_.push_back(std::move(device));
  return ptr;
}

void FastPairDeviceRepository::RemoveDevice(const FastPairDevice* device) {
  devices_.erase(
      std::remove_if(devices_.begin(), devices_.end(),
                     [&](const std::unique_ptr<FastPairDevice>& item) {
                       return item.get() == device;
                     }),
      devices_.end());
}

std::optional<FastPairDevice*> FastPairDeviceRepository::FindDevice(
    absl::string_view mac_address) {
  auto it = std::find_if(devices_.begin(), devices_.end(),
                         [&](const std::unique_ptr<FastPairDevice>& device) {
                           return device->GetBleAddress() == mac_address ||
                                  device->GetPublicAddress() == mac_address;
                         });
  if (it != devices_.end()) {
    return it->get();
  } else {
    return std::nullopt;
  }
}

}  // namespace fastpair
}  // namespace nearby
