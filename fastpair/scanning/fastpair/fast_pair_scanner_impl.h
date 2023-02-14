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

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "fastpair/internal/ble/ble.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/task_runner.h"

namespace nearby {
namespace fastpair {

class FastPairScannerImpl : public FastPairScanner {
 public:
  class Factory {
   public:
    static std::shared_ptr<FastPairScanner> Create();

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory();
    virtual std::shared_ptr<FastPairScanner> CreateInstance() = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairScannerImpl();
  FastPairScannerImpl(const FastPairScannerImpl&) = delete;
  FastPairScannerImpl& operator=(const FastPairScannerImpl&) = delete;
  ~FastPairScannerImpl() override = default;

  // FastPairScanner::Observer
  void AddObserver(FastPairScanner::Observer* observer) override;
  void RemoveObserver(FastPairScanner::Observer* observer) override;

  void StartScanning();
  void StopScanning();

  // Fast Pair discovered peripheral callback
  void OnDeviceFound(const BlePeripheral& peripheral);
  void OnDeviceLost(const BlePeripheral& peripheral);

  void NotifyDeviceFound(const BlePeripheral& peripheral);
  // Todo(b/267348348): Support Flags to control feature ramp
  bool IsFastPairLowPowerEnabled() const { return false; }

  // For unit test
  Ble& GetBle() { return ble_; }

 private:
  std::shared_ptr<TaskRunner> task_runner_;

  // Map of a Bluetooth device address to a set of advertisement data we have
  // seen.
  absl::flat_hash_map<std::string, std::set<ByteArray>>
      device_address_advertisement_data_map_;

  BluetoothAdapter bluetooth_adapter_;
  Ble ble_;
  ObserverList<FastPairScanner::Observer> observer_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_IMPL_H_
