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
#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner_impl.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/logging.h"
#include "internal/platform/task_runner_impl.h"

namespace nearby {
namespace fastpair {
ScannerBrokerImpl::ScannerBrokerImpl() {
  adapter_ = std::make_shared<BluetoothAdapter>();
  task_runner_ = std::make_shared<TaskRunnerImpl>(1);
}

void ScannerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScannerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScannerBrokerImpl::StartScanning(Protocol protocol) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": protocol=" << protocol;
  task_runner_->PostTask([this]() { StartFastPairScanning(); });
}

void ScannerBrokerImpl::StopScanning(Protocol protocol) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": protocol=" << protocol;
  task_runner_->PostTask([this]() { StopFastPairScanning(); });
}
void ScannerBrokerImpl::StartFastPairScanning() {
  DCHECK(!fast_pair_discoverable_scanner_);
  DCHECK(adapter_);
  NEARBY_LOGS(VERBOSE) << "Starting Fast Pair Scanning.";
  scanner_impl_ = std::make_shared<FastPairScannerImpl>();
  scanner_impl_->StartScanning();
  scanner_ = std::move(scanner_impl_);

  fast_pair_discoverable_scanner_ =
      FastPairDiscoverableScannerImpl::Factory::Create(
          scanner_, adapter_,
          absl::bind_front(&ScannerBrokerImpl::NotifyDeviceFound, this),
          absl::bind_front(&ScannerBrokerImpl::NotifyDeviceLost, this));
}

void ScannerBrokerImpl::StopFastPairScanning() {
  fast_pair_discoverable_scanner_.reset();
  observers_.Clear();
  NEARBY_LOGS(VERBOSE) << __func__ << "Stopping Fast Pair Scanning.";
}

void ScannerBrokerImpl::NotifyDeviceFound(const FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": Notifying device found, model id = "
                    << device.model_id;
  for (auto& observer : observers_) {
    observer->OnDeviceFound(device);
  }
}

void ScannerBrokerImpl::NotifyDeviceLost(const FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": Notifying device lost, model id = "
                    << device.model_id;
  for (auto& observer : observers_) {
    observer->OnDeviceLost(device);
  }
}

}  // namespace fastpair
}  // namespace nearby
