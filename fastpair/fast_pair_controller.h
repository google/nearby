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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_H_

#include <functional>
#include <string>

namespace nearby {
namespace fastpair {

class FastPairController {
 public:
  enum class StatusCodes {
    // The operation successed.
    kOk = 0,
    // The operation failed.
    kError = 1,
  };

  static std::string StatusCodeToString(StatusCodes status_code);

  virtual ~FastPairController() = default;

  // Obtain the scanning status
  virtual bool IsScanning() = 0;

  // Obtain the pairing status
  virtual bool IsPairing() = 0;

  // Server Access trigger just for adapter implementation testing, will be
  // delete after success
  virtual bool IsServerAccessing() = 0;

  // Trigger function of the scanning
  virtual void StartScan() = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FAST_PAIR_FASTPAIR_CONTROLLER_H_
