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

#include "fastpair/dart/windows/fast_pair_wrapper_adapter.h"

#include "fastpair/dart/fast_pair_wrapper.h"
#include "fastpair/dart/fast_pair_wrapper_impl.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace windows {

static FastPairWrapper *pWrapper_ = nullptr;
void *InitFastPairWrapper() {
#if defined(NEARBY_LOG_SEVERITY)
  // Direct override of logging level.
  NEARBY_LOG_SET_SEVERITY(NEARBY_LOG_SEVERITY);
#endif  // LOG_SEVERITY_VERBOSE;

  FastPairWrapperImpl *pWrapper = new FastPairWrapperImpl();
  pWrapper_ = pWrapper;
  return pWrapper;
}

void CloseFastPairWrapper(FastPairWrapper *pWrapper) {
  NEARBY_LOGS(INFO) << "[[ Closing Fast Pair Wrapper. ]]";
  if (pWrapper_ != nullptr) delete pWrapper_;
  NEARBY_LOGS(INFO) << "[[ Successfully closed Fast Pair Wrapper. ]]";
}

void __stdcall StartScan(FastPairWrapper *pWrapper) {
  NEARBY_LOGS(VERBOSE) << "StartScan is called";
  if (pWrapper_ == nullptr) {
    NEARBY_LOGS(INFO) << "The pWrapper is a null pointer.";
    return;
  }
  return pWrapper_->StartScan();
}

bool __stdcall IsScanning(FastPairWrapper *pWrapper) {
  if (pWrapper_ == nullptr) {
    return false;
  }

  return static_cast<FastPairWrapper *>(pWrapper_)->IsScanning();
}

bool __stdcall IsPairing(FastPairWrapper *pWrapper) {
  if (pWrapper_ == nullptr) {
    return false;
  }
  // return connecting
  return static_cast<FastPairWrapper *>(pWrapper_)->IsPairing();
}

bool __stdcall IsServerAccessing(FastPairWrapper *pWrapper) {
  if (pWrapper_ == nullptr) {
    return false;
  }
  return static_cast<FastPairWrapper *>(pWrapper_)->IsServerAccessing();
}

}  // namespace windows
}  // namespace fastpair
}  // namespace nearby
