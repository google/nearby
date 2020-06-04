// Copyright 2020 Google LLC
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

#ifndef PLATFORM_EXCEPTION_H_
#define PLATFORM_EXCEPTION_H_

#include <utility>

namespace location {
namespace nearby {

struct Exception {
  enum Value : int {
    NONE,
    IO,
    INTERRUPTED,
    INVALID_PROTOCOL_BUFFER,
    EXECUTION,
    // New code should use the kConstants.
    // Old CONSTANTS are deprecated, and should not be used.
    kFailed = -1,     // Initial value of Exception; any unknown error.
    kSuccess = NONE,  // No exception.
    kIo = IO,         // IO Error happened.
    kInterrupted = INTERRUPTED,  // Operation was interrupted.
    kInvalidProtocolBuffer = INVALID_PROTOCOL_BUFFER,  // Couldn't parse.
    kExecution = EXECUTION,                            // Couldn't execute.
    kTimeout,  // Operation did not finish within specified time.
  };
  Value value {kFailed};
};

// ExceptionOr provides experience similar to StatusOr<T> used in
// Google Cloud API, see:
// https://googleapis.github.io/google-cloud-cpp/0.7.0/common/status__or_8h_source.html
//
// If ok() returns true, result() is a usable return value. Otherwise,
// exception() explains why such a value is not present.
//
// A typical pattern of usage is as follows:
//
//      if (!e.ok()) {
//        if (Exception::EXCEPTION_TYPE_1 == e.exception()) {
//          // Handle Exception::EXCEPTION_TYPE_1.
//        } else if (Exception::EXCEPTION_TYPE_2 == e.exception()) {
//          // Handle Exception::EXCEPTION_TYPE_2.
//        }
//
//         return;
//      }
//
//      // Use e.result().
template <typename T>
class ExceptionOr {
 public:
  ExceptionOr() = default;
  ExceptionOr(T&& result) : result_{std::move(result)},  // NOLINT
                            exception_{Exception::kSuccess} {}
  ExceptionOr(const T& result) : result_{result},  // NOLINT
                                 exception_{Exception::kSuccess} {}
  ExceptionOr(Exception::Value exception) : exception_{exception} {}  // NOLINT

  bool ok() const { return exception_.value == Exception::kSuccess; }

  T& result() & { return result_; }
  const T& result() const & { return result_; }
  T&& result() && { return std::move(result_); }
  const T&& result() const && { return std::move(result_); }

  Exception::Value exception() const { return exception_.value; }

  T GetResult() const;
  Exception GetException() const;

 private:
  T result_;
  Exception exception_ {Exception::kFailed};
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_EXCEPTION_H_
