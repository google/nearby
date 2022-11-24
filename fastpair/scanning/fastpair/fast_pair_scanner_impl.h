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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_IMPL_H_

#include <string>

#include "fastpair/internal/ble/ble.h"
#include "internal/platform/bluetooth_adapter.h"

namespace location {
namespace nearby {
namespace fastpair {

class FastPairScannerImpl {
 public:
  FastPairScannerImpl() = default;
  FastPairScannerImpl(const FastPairScannerImpl&) = default;
  FastPairScannerImpl& operator=(const FastPairScannerImpl&) = default;

  ~FastPairScannerImpl() = default;

  bool StartScanning();
  bool StopScanning();

  void OnDeviceFound(BlePeripheral& peripheral, const std::string& service_id,
                     const ByteArray& advertisement_bytes,
                     bool fast_advertisement);
  void OnDeviceLost(BlePeripheral& peripheral,
                         const std::string& service_id);

 private:
  Ble ble_;
};

}  // namespace fastpair
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_IMPL_H_
