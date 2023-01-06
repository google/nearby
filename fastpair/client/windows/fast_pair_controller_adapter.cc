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

#include "fastpair/client/windows/fast_pair_controller_adapter.h"

#include "fastpair/fast_pair_controller.h"
#include "fastpair/fast_pair_controller_impl.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace windows {

static FastPairController *pController_ = nullptr;
void *InitFastPairController() {
  FastPairControllerImpl *pController = new FastPairControllerImpl();
  pController_ = pController;
  return pController;
}

void CloseFastPairController(FastPairController *pController) {
  NEARBY_LOGS(INFO) << "[[ Closing Fast Pair Controller. ]]";
  if (pController_ != nullptr) delete pController_;
  NEARBY_LOGS(INFO) << "[[ Successfully closed Fast Pair Controller. ]]";
}

void __stdcall StartScan(FastPairController *pController) {
  NEARBY_LOGS(INFO) << "StartScan is called";
  if (pController_ == nullptr) {
    NEARBY_LOGS(INFO) << "The pController is a null pointer.";
    return;
  }
  return pController_->StartScan();
}

bool __stdcall IsScanning(FastPairController *pController) {
  if (pController_ == nullptr) {
    return false;
  }

  return static_cast<FastPairController *>(pController_)->IsScanning();
}

bool __stdcall IsPairing(FastPairController *pController) {
  if (pController_ == nullptr) {
    return false;
  }
  // return connecting
  return static_cast<FastPairController *>(pController_)->IsPairing();
}

bool __stdcall IsServerAccessing(FastPairController *pController) {
  if (pController_ == nullptr) {
    return false;
  }
  return static_cast<FastPairController *>(pController_)->IsServerAccessing();
}

}  // namespace windows
}  // namespace fastpair
}  // namespace nearby
