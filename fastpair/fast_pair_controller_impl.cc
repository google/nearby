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

#include "fastpair/fast_pair_controller_impl.h"

#include <memory>

#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

FastPairControllerImpl::FastPairControllerImpl() {
  NEARBY_LOGS(INFO) << "FastPairControllerImpl starts.";
}
FastPairControllerImpl::~FastPairControllerImpl() = default;

void FastPairControllerImpl::StartScan() {
  scanner_ = std::make_unique<FastPairScannerImpl>();
  if (is_scanning_) {
    NEARBY_LOGS(VERBOSE) << __func__ << ": We're currently scanning. ";
    return;
  }
  is_scanning_ = true;
  scanner_->StartScanning();
}

bool FastPairControllerImpl::IsScanning() { return is_scanning_; }

bool FastPairControllerImpl::IsPairing() { return is_connecting_; }

bool FastPairControllerImpl::IsServerAccessing() { return is_server_access_; }

}  // namespace fastpair
}  // namespace nearby
