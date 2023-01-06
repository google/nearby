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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_IMPL_H_

#include <functional>
#include <memory>

#include "fastpair/fast_pair_controller.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"

namespace nearby {
namespace fastpair {

class FastPairScannerImpl;

class FastPairControllerImpl : public FastPairController,
                               public FastPairScannerImpl {
 public:
  explicit FastPairControllerImpl();
  ~FastPairControllerImpl() override;

  bool IsScanning() override;
  bool IsPairing() override;
  bool IsServerAccessing() override;
  void StartScan() override;

 private:
  std::unique_ptr<FastPairScannerImpl> scanner_;

  // True if we are currently scanning for remote devices.
  bool is_scanning_ = false;
  bool is_connecting_ = false;
  bool is_server_access_ = false;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_IMPL_H_
