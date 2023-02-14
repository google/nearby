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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_SCANNER_BROKER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_SCANNER_BROKER_IMPL_H_

#include <functional>
#include <memory>
#include <vector>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"
#include "fastpair/scanning/scanner_broker.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/task_runner.h"

namespace nearby {
namespace fastpair {

class ScannerBrokerImpl : public ScannerBroker {
 public:
  explicit ScannerBrokerImpl();
  ~ScannerBrokerImpl() override = default;

  // ScannerBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void StartScanning(Protocol protocol) override;
  void StopScanning(Protocol protocol) override;

 private:
  void StartFastPairScanning();
  void StopFastPairScanning();
  void NotifyDeviceFound(const FastPairDevice& device);
  void NotifyDeviceLost(const FastPairDevice& device);

  std::shared_ptr<TaskRunner> task_runner_;
  std::shared_ptr<FastPairScanner> scanner_;
  std::shared_ptr<FastPairScannerImpl> scanner_impl_;
  std::shared_ptr<BluetoothAdapter> adapter_;
  std::unique_ptr<FastPairDiscoverableScanner> fast_pair_discoverable_scanner_;
  ObserverList<Observer> observers_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_SCANNER_BROKER_IMPL_H_
