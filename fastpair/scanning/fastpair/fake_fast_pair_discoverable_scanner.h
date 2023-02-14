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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAKE_FAST_PAIR_DISCOVERABLE_SCANNER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAKE_FAST_PAIR_DISCOVERABLE_SCANNER_H_

#include <utility>

#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"

namespace nearby {
namespace fastpair {

class FakeFastPairDiscoverableScanner : public FastPairDiscoverableScanner {
 public:
  FakeFastPairDiscoverableScanner(DeviceCallback found_callback,
                                  DeviceCallback lost_callback)
      : found_callback_(std::move(found_callback)),
        lost_callback_(std::move(lost_callback)) {}

  ~FakeFastPairDiscoverableScanner() override = default;

  void TriggerDeviceFoundCallback(FastPairDevice& device) {
    found_callback_(device);
  }

  void TriggerDeviceLostCallback(FastPairDevice& device) {
    lost_callback_(device);
  }

 private:
  DeviceCallback found_callback_;
  DeviceCallback lost_callback_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAKE_FAST_PAIR_DISCOVERABLE_SCANNER_H_
