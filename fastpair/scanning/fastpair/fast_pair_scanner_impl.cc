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

#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"

#include <string>

#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/constant.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace fastpair {

bool FastPairScannerImpl::StartScanning() {
  if (ble_.Enable() &&
      ble_.StartScanning(kServiceId,  kFastPairServiceUuid,
                         {
                             .peripheral_discovered_cb = absl::bind_front(
                                 &FastPairScannerImpl::OnDeviceFound, this),
                             .peripheral_lost_cb = absl::bind_front(
                                 &FastPairScannerImpl::OnDeviceLost, this),
                         })) {
    NEARBY_LOGS(INFO)
        << "Started scanning for BLE advertisements for service_id="
        << kServiceId;
    return true;
  } else {
    NEARBY_LOGS(INFO) << "Couldn't start scanning on BLE for service_id="
                      << kServiceId;
    return false;
  }
}

void FastPairScannerImpl::OnDeviceFound(BlePeripheral& peripheral,
                                        const std::string& service_id,
                                        const ByteArray& advertisement_bytes,
                                        bool fast_advertisement) {
  if (advertisement_bytes.size() == 3) {
    NEARBY_LOGS(VERBOSE) << "Fast Pair 0xFE2C Advertisement discovered. ";
    NEARBY_LOGS(VERBOSE) << "BluetoothAddress is " << peripheral.GetName()
                         << ", Model id = "
                         << absl::BytesToHexString(
                                advertisement_bytes.AsStringView());
  }
}

void FastPairScannerImpl::OnDeviceLost(BlePeripheral& peripheral,
                                            const std::string& service_id) {
  NEARBY_LOGS(INFO) << "Lost BlePeripheral with BluetoothAddress is "
                    << peripheral.GetName();
}

}  // namespace fastpair
}  // namespace nearby
}  // namespace location
