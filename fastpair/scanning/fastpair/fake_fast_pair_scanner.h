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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAKE_FAST_PAIR_SCANNER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAKE_FAST_PAIR_SCANNER_H_

#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/base/observer_list.h"

namespace nearby {
namespace fastpair {

class FakeFastPairScanner final : public FastPairScanner {
 public:
  FakeFastPairScanner() = default;
  FakeFastPairScanner(const FakeFastPairScanner&) = delete;
  FakeFastPairScanner& operator=(const FakeFastPairScanner&) = delete;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyDeviceFound(const BlePeripheral& peripheral);
  void NotifyDeviceLost(const BlePeripheral& peripheral);

 private:
  ~FakeFastPairScanner() override = default;

  ObserverList<FastPairScanner::Observer> observer_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAKE_FAST_PAIR_SCANNER_H_
