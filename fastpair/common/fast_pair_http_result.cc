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

#include "fastpair/common/fast_pair_http_result.h"

#include <ostream>
#include <string>

#include "absl/status/status.h"

namespace nearby {
namespace fastpair {

FastPairHttpError FastPairHttpErrorForHttpResponseCode(absl::Status status) {
  switch (status.code()) {
    case absl::StatusCode::kOk:
      return FastPairHttpError::kSuccess;
    case absl::StatusCode::kInvalidArgument:
      return FastPairHttpError::kHttpErrorInvalidArgument;
    case absl::StatusCode::kUnauthenticated:
      return FastPairHttpError::kHttpErrorUnauthenticated;
    case absl::StatusCode::kPermissionDenied:
      return FastPairHttpError::kHttpErrorPermissionDenied;
    case absl::StatusCode::kDeadlineExceeded:
      return FastPairHttpError::kHttpErrorDeadlineExceeded;
    case absl::StatusCode::kUnavailable:
      return FastPairHttpError::kHttpErrorUnavailable;
    case absl::StatusCode::kUnknown:
      return FastPairHttpError::kHttpErrorUnknown;
    case absl::StatusCode::kInternal:
      return FastPairHttpError::kHttpErrorInternal;
    case absl::StatusCode::kCancelled:
    case absl::StatusCode::kUnimplemented:
    case absl::StatusCode::kOutOfRange:
    case absl::StatusCode::kAborted:
    case absl::StatusCode::kNotFound:
      return FastPairHttpError::kHttpErrorOtherFailure;
    default:
      break;
  }
  return FastPairHttpError::kHttpErrorUnknown;
}

FastPairHttpStatus::FastPairHttpStatus(const absl::Status& absl_status)
    : absl_status_(absl_status) {
  if (absl_status_.ok()) {
    status_ = Status::kSuccess;
  } else {
    if (absl_status_.code() == absl::StatusCode::kFailedPrecondition) {
      status_ = Status::kNetworkFailure;
    } else {
      status_ = Status::kHttpFailure;
    }
  }
}

FastPairHttpStatus::FastPairHttpStatus(const FastPairHttpStatus& status) =
    default;

FastPairHttpStatus::~FastPairHttpStatus() = default;

bool FastPairHttpStatus::IsSuccess() const {
  return status_ == Status::kSuccess;
}

std::string FastPairHttpStatus::ToString() const {
  std::string status;
  switch (status_) {
    case Status::kSuccess:
      status = "kSuccess";
      break;
    case Status::kNetworkFailure:
      status = "kNetworkFailure";
      break;
    case Status::kHttpFailure:
      status = "kHttpFailure";
      break;
  }

  return "status=" + status;
}

std::ostream& operator<<(std::ostream& stream, const FastPairHttpError& error) {
  switch (error) {
    case FastPairHttpError::kSuccess:
      stream << "[Success]";
      break;
    case FastPairHttpError::kTimeout:
      stream << "[Timeout]";
      break;
    case FastPairHttpError::kUnknown:
      stream << "[Unknown]";
      break;
    case FastPairHttpError::kAuthenticationError:
      stream << "[Authentication]";
      break;
    case FastPairHttpError::kHttpErrorUnknown:
      stream << "[HTTP Error: Unknown]";
      break;
    case FastPairHttpError::kHttpErrorDeadlineExceeded:
      stream << "[HTTP Error: Deadline exceeded]";
      break;
    case FastPairHttpError::kHttpErrorPermissionDenied:
      stream << "[HTTP Error: Permission denied]";
      break;
    case FastPairHttpError::kHttpErrorUnavailable:
      stream << "[HTTP Error: Unavailable]";
      break;
    case FastPairHttpError::kHttpErrorUnauthenticated:
      stream << "[HTTP Error: Unauthenticated]";
      break;
    case FastPairHttpError::kHttpErrorInvalidArgument:
      stream << "[HTTP Error: Invalid argument]";
      break;
    case FastPairHttpError::kHttpErrorOffline:
      stream << "[HTTP Error: offline]";
      break;
    case FastPairHttpError::kHttpErrorInternal:
      stream << "[HTTP Error: Internal server error]";
      break;
    case FastPairHttpError::kHttpErrorOtherFailure:
      stream << "[HTTP Error: Other failure]";
      break;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const FastPairHttpStatus& status) {
  stream << status.ToString();
  return stream;
}

}  // namespace fastpair
}  // namespace nearby
