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

#include <memory>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_non_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "fastpair/scanning/scanner_broker.h"
#include "internal/base/observer_list.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

class ScannerBrokerImpl : public ScannerBroker {
 public:
  ScannerBrokerImpl(Mediums& mediums, SingleThreadExecutor* executor,
                    FastPairDeviceRepository* device_repository);
  ~ScannerBrokerImpl() override = default;

  // ScannerBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::unique_ptr<ScanningSession> StartScanning(Protocol protocol) override;
  void StopScanning(Protocol protocol);

 private:
  void NotifyDeviceFound(FastPairDevice& device);
  void NotifyDeviceLost(FastPairDevice& device);

  Mediums& mediums_;
  SingleThreadExecutor* executor_;
  std::unique_ptr<FastPairScanner> scanner_;
  std::unique_ptr<FastPairDiscoverableScanner> fast_pair_discoverable_scanner_;
  std::unique_ptr<FastPairNonDiscoverableScanner>
      fast_pair_non_discoverable_scanner_;
  ObserverList<Observer> observers_;
  FastPairDeviceRepository* device_repository_;
  std::unique_ptr<FastPairScanner::ScanningSession> scanning_session_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_SCANNER_BROKER_IMPL_H_
