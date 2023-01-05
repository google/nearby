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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_HTTP_RESULT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_HTTP_RESULT_H_

#include <ostream>
#include <string>

#include "absl/status/status.h"

namespace nearby {
namespace fastpair {

enum class FastPairHttpError {
  kSuccess,
  kTimeout,
  kUnknown,
  kAuthenticationError,
  kHttpErrorUnknown,
  kHttpErrorDeadlineExceeded,
  kHttpErrorPermissionDenied,
  kHttpErrorUnavailable,
  kHttpErrorUnauthenticated,
  kHttpErrorInvalidArgument,
  kHttpErrorOffline,
  kHttpErrorInternal,
  kHttpErrorOtherFailure
};

class FastPairHttpStatus {
 public:
  explicit FastPairHttpStatus(const absl::Status& absl_status);
  FastPairHttpStatus(const FastPairHttpStatus& status);
  ~FastPairHttpStatus();

  bool IsSuccess() const;
  std::string ToString() const;

 private:
  enum class Status { kSuccess, kNetworkFailure, kHttpFailure } status_;
  absl::Status absl_status_;
};

FastPairHttpError FastPairHttpErrorForHttpResponseCode(absl::Status status);

std::ostream& operator<<(std::ostream& stream, const FastPairHttpError& error);
std::ostream& operator<<(std::ostream& stream,
                         const FastPairHttpStatus& status);

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_HTTP_RESULT_H_
