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

#include "fastpair/dart/fast_pair_wrapper_impl.h"

#include <memory>


#include "fastpair/scanning/scanner_broker_impl.h"
#include "internal/platform/logging.h"
#include "fastpair/common/protocol.h"

namespace nearby {
namespace fastpair {

FastPairWrapperImpl::FastPairWrapperImpl() {
  NEARBY_LOGS(INFO) << "FastPairWrapperImpl starts.";
}
FastPairWrapperImpl::~FastPairWrapperImpl() = default;

void FastPairWrapperImpl::StartScan() {
  scanner_broker_ = std::make_unique<ScannerBrokerImpl>();
  if (is_scanning_) {
    NEARBY_LOGS(VERBOSE) << __func__ << ": We're currently scanning. ";
    return;
  }
  is_scanning_ = true;
  scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
}

bool FastPairWrapperImpl::IsScanning() { return is_scanning_; }

bool FastPairWrapperImpl::IsPairing() { return is_connecting_; }

bool FastPairWrapperImpl::IsServerAccessing() { return is_server_access_; }

}  // namespace fastpair
}  // namespace nearby
