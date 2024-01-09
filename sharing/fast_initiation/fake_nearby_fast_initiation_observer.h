// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_FAKE_NEARBY_FAST_INITIATION_OBSERVER_H_
#define THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_FAKE_NEARBY_FAST_INITIATION_OBSERVER_H_

#include "sharing/fast_initiation/nearby_fast_initiation.h"

namespace nearby {
namespace sharing {

class FakeNearbyFastInitiationObserver : public NearbyFastInitiation::Observer {
 public:
  explicit FakeNearbyFastInitiationObserver(
      NearbyFastInitiation* fast_init_manager) {
    fast_init_manager_ = fast_init_manager;
    num_hardware_error_reported_ = 0;
  }

  void HardwareErrorReported(NearbyFastInitiation* fast_init_manager) override {
    if (fast_init_manager_ == fast_init_manager) {
      num_hardware_error_reported_ += 1;
    }
  }

  int GetNumHardwareErrorReported() { return num_hardware_error_reported_; }

 private:
  NearbyFastInitiation* fast_init_manager_;
  int num_hardware_error_reported_;
};

}  // namespace sharing
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_FAKE_NEARBY_FAST_INITIATION_OBSERVER_H_
