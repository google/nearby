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

#include "fastpair/keyed_service/fast_pair_mediator.h"

#include <memory>
#include <utility>

#include "fastpair/common/protocol.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
Mediator::Mediator(std::unique_ptr<ScannerBroker> scanner_broker,
                   std::unique_ptr<FastPairRepository> fast_pair_repository)
    : scanner_broker_(std::move(scanner_broker)),
      fast_pair_repository_(std::move(fast_pair_repository)) {
  scanner_broker_->AddObserver(this);
}

void Mediator::OnDeviceFound(const FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
  // Show discovery notification
}

void Mediator::OnDeviceLost(const FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
}

void Mediator::StartScanning() {
  if (IsFastPairEnabled()) {
    scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
    return;
  }
  scanner_broker_->StopScanning(Protocol::kFastPairInitialPairing);
}

bool Mediator::IsFastPairEnabled() {
  // TODO(b/275452353): Add  feature_status_tracker IsFastPairEnabled()
  // Currently default to true.
  NEARBY_LOGS(VERBOSE) << __func__ << ": " << true;
  return true;
}
}  // namespace fastpair
}  // namespace nearby
