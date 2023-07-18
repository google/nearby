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

#include "fastpair/scanning/scanner_broker_impl.h"

#include <memory>
#include <utility>

#include "absl/functional/bind_front.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_non_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {

class ScanningSessionImpl : public ScannerBroker::ScanningSession {
 public:
  ScanningSessionImpl(ScannerBrokerImpl* scanner, Protocol protocol)
      : scanner_(scanner), protocol_(protocol) {}
  ~ScanningSessionImpl() override { scanner_->StopScanning(protocol_); }

 private:
  ScannerBrokerImpl* scanner_;
  Protocol protocol_;
};

}  // namespace

ScannerBrokerImpl::ScannerBrokerImpl(
    Mediums& mediums, SingleThreadExecutor* executor,
    FastPairDeviceRepository* device_repository)
    : mediums_(mediums),
      executor_(executor),
      device_repository_(device_repository) {}

void ScannerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScannerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ScannerBroker::ScanningSession>
ScannerBrokerImpl::StartScanning(Protocol protocol) {
  DCHECK(!fast_pair_discoverable_scanner_);
  DCHECK(!fast_pair_non_discoverable_scanner_);
  NEARBY_LOGS(VERBOSE) << "Starting Fast Pair Scanning.";
  scanner_ = std::make_unique<FastPairScannerImpl>(mediums_, executor_);
  fast_pair_discoverable_scanner_ =
      FastPairDiscoverableScanner::Factory::Create(
          *scanner_,
          absl::bind_front(&ScannerBrokerImpl::NotifyDeviceFound, this),
          absl::bind_front(&ScannerBrokerImpl::NotifyDeviceLost, this),
          executor_, device_repository_);

  fast_pair_non_discoverable_scanner_ =
      FastPairNonDiscoverableScanner::Factory::Create(
          *scanner_,
          absl::bind_front(&ScannerBrokerImpl::NotifyDeviceFound, this),
          absl::bind_front(&ScannerBrokerImpl::NotifyDeviceLost, this),
          executor_, device_repository_);
  scanning_session_ = scanner_->StartScanning();
  return std::make_unique<ScanningSessionImpl>(this, protocol);
}

void ScannerBrokerImpl::StopScanning(Protocol protocol) {
  NEARBY_LOGS(VERBOSE) << __func__ << " Stopping Fast Pair Scanning.";
  scanning_session_.reset();
  observers_.Clear();

  DestroyOnExecutor(std::move(fast_pair_discoverable_scanner_), executor_);
  DestroyOnExecutor(std::move(fast_pair_non_discoverable_scanner_), executor_);
  DestroyOnExecutor(std::move(scanner_), executor_);
}

void ScannerBrokerImpl::NotifyDeviceFound(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": Notifying device found, model id = "
                    << device.GetModelId();
  for (auto& observer : observers_.GetObservers()) {
    observer->OnDeviceFound(device);
  }
}

void ScannerBrokerImpl::NotifyDeviceLost(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": Notifying device lost, model id = "
                    << device.GetModelId();
  for (auto& observer : observers_.GetObservers()) {
    observer->OnDeviceLost(device);
  }
}

}  // namespace fastpair
}  // namespace nearby
