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

#include "internal/network/http_result.h"

#include <ostream>
#include <string>

#include "absl/status/status.h"

namespace nearby {
namespace network {

HttpError HttpErrorForHttpResponseCode(absl::Status status) {
  switch (status.code()) {
    case absl::StatusCode::kOk:
      return HttpError::kSuccess;
    case absl::StatusCode::kInvalidArgument:
      return HttpError::kHttpErrorInvalidArgument;
    case absl::StatusCode::kUnauthenticated:
      return HttpError::kHttpErrorUnauthenticated;
    case absl::StatusCode::kPermissionDenied:
      return HttpError::kHttpErrorPermissionDenied;
    case absl::StatusCode::kDeadlineExceeded:
      return HttpError::kHttpErrorDeadlineExceeded;
    case absl::StatusCode::kUnavailable:
      return HttpError::kHttpErrorUnavailable;
    case absl::StatusCode::kUnknown:
      return HttpError::kHttpErrorUnknown;
    case absl::StatusCode::kInternal:
      return HttpError::kHttpErrorInternal;
    case absl::StatusCode::kCancelled:
    case absl::StatusCode::kUnimplemented:
    case absl::StatusCode::kOutOfRange:
    case absl::StatusCode::kAborted:
    case absl::StatusCode::kNotFound:
      return HttpError::kHttpErrorOtherFailure;
    default:
      break;
  }
  return HttpError::kUnknown;
}

HttpStatus::HttpStatus(const absl::Status& absl_status)
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

HttpStatus::HttpStatus(const HttpStatus& status) = default;

HttpStatus::~HttpStatus() = default;

bool HttpStatus::IsSuccess() const { return status_ == Status::kSuccess; }

std::string HttpStatus::ToString() const {
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

std::ostream& operator<<(std::ostream& stream, const HttpError& error) {
  switch (error) {
    case HttpError::kSuccess:
      stream << "[Success]";
      break;
    case HttpError::kTimeout:
      stream << "[Timeout]";
      break;
    case HttpError::kUnknown:
      stream << "[Unknown]";
      break;
    case HttpError::kAuthenticationError:
      stream << "[Authentication]";
      break;
    case HttpError::kHttpErrorUnknown:
      stream << "[HTTP Error: Unknown]";
      break;
    case HttpError::kHttpErrorDeadlineExceeded:
      stream << "[HTTP Error: Deadline exceeded]";
      break;
    case HttpError::kHttpErrorPermissionDenied:
      stream << "[HTTP Error: Permission denied]";
      break;
    case HttpError::kHttpErrorUnavailable:
      stream << "[HTTP Error: Unavailable]";
      break;
    case HttpError::kHttpErrorUnauthenticated:
      stream << "[HTTP Error: Unauthenticated]";
      break;
    case HttpError::kHttpErrorInvalidArgument:
      stream << "[HTTP Error: Invalid argument]";
      break;
    case HttpError::kHttpErrorOffline:
      stream << "[HTTP Error: offline]";
      break;
    case HttpError::kHttpErrorInternal:
      stream << "[HTTP Error: Internal server error]";
      break;
    case HttpError::kHttpErrorOtherFailure:
      stream << "[HTTP Error: Other failure]";
      break;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream, const HttpStatus& status) {
  stream << status.ToString();
  return stream;
}

}  // namespace network
}  // namespace nearby
