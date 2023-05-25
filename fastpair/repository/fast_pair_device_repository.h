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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_DEVICE_REPOSITORY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_DEVICE_REPOSITORY_H_

#include <memory>
#include <optional>
#include <vector>

#include "fastpair/common/fast_pair_device.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

// Owner of `FastPairDevice` instances.
class FastPairDeviceRepository {
 public:
  explicit FastPairDeviceRepository(SingleThreadExecutor* executor)
      : executor_(executor) {}

  // Adds device to the repository and takes over ownership.
  // If a device with the same MAC address is already in the repository, it is
  // replaced.
  // Returns a stable, non-null pointer to the inserted element. The pointer is
  // valid until `RemoveDevice()`.
  FastPairDevice* AddDevice(std::unique_ptr<FastPairDevice> device);

  // Removes the device and frees resources.
  void RemoveDevice(const FastPairDevice* device);

  // Finds a device matching the mac address. The mac address can be either BT
  // or BLE.
  std::optional<FastPairDevice*> FindDevice(absl::string_view mac_address);

 private:
  // Removes `device` from `devices_`.
  std::unique_ptr<FastPairDevice> ExtractDevice(const FastPairDevice* device);
  Mutex mutex_;
  SingleThreadExecutor* executor_;
  std::vector<std::unique_ptr<FastPairDevice>> devices_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_DEVICE_REPOSITORY_H_
