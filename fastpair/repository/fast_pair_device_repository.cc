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

#include "fastpair/common/account_key.h"
#include "fastpair/common/fast_pair_device.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace fastpair {

FastPairDevice* FastPairDeviceRepository::AddDevice(
    std::unique_ptr<FastPairDevice> device) {
  const auto& id = device->GetUniqueId();
  MutexLock lock(&mutex_);
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
  std::unique_ptr<FastPairDevice> fast_pair_device = ExtractDevice(device);
  if (fast_pair_device == nullptr) return;
  // Tasks running in the background may still be referencing `device`.
  // Deferring the destruction to the background thread should prevent
  // use-after-free errors.
  executor_->Execute([this, fast_pair_device = std::move(fast_pair_device)]() {
    for (auto* callback : observers_.GetObservers()) {
      (*callback)(*fast_pair_device);
    }
    NEARBY_LOGS(VERBOSE) << "Destroyed FP device: " << *fast_pair_device;
  });
}

std::optional<FastPairDevice*> FastPairDeviceRepository::FindDevice(
    absl::string_view mac_address) {
  MutexLock lock(&mutex_);
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

std::optional<FastPairDevice*> FastPairDeviceRepository::FindDevice(
    const AccountKey& account_key) {
  MutexLock lock(&mutex_);
  auto it = std::find_if(devices_.begin(), devices_.end(),
                         [&](const std::unique_ptr<FastPairDevice>& device) {
                           return device->GetAccountKey() == account_key;
                         });
  if (it != devices_.end()) {
    return it->get();
  } else {
    return std::nullopt;
  }
}

std::unique_ptr<FastPairDevice> FastPairDeviceRepository::ExtractDevice(
    const FastPairDevice* device) {
  MutexLock lock(&mutex_);
  auto it = std::find_if(devices_.begin(), devices_.end(),
                         [&](const std::unique_ptr<FastPairDevice>& item) {
                           return item.get() == device;
                         });
  if (it == devices_.end()) return nullptr;
  std::unique_ptr<FastPairDevice> fast_pair_device = std::move(*it);
  devices_.erase(it);
  return fast_pair_device;
}

}  // namespace fastpair
}  // namespace nearby
