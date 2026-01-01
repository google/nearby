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

#ifndef PLATFORM_BASE_EXCEPTION_H_
#define PLATFORM_BASE_EXCEPTION_H_

#include <utility>

#include "absl/meta/type_traits.h"

namespace nearby {

struct Exception {
  enum Value : int {
    kFailed = -1,      // Initial value of Exception; any unknown error.
    kSuccess = 0,      // No exception.
    kIo = 1,           // IO Error happened.
    kInterrupted = 2,  // Operation was interrupted.
    kInvalidProtocolBuffer = 3,  // Couldn't parse.
    kExecution = 4,              // Couldn't execute.
    kTimeout = 5,            // Operation did not finish within specified time.
    kIllegalCharacters = 6,  // File name or parent path contained
                             // illegal chars
  };
  bool Ok() const { return value == kSuccess; }
  explicit operator bool() const { return Ok(); }
  bool Raised() const { return !Ok(); }
  bool Raised(Value val) const { return value == val; }
  Value value{kFailed};
};

constexpr inline bool operator==(const Exception& a, const Exception& b) {
  return a.value == b.value;
}

constexpr inline bool operator!=(const Exception& a, const Exception& b) {
  return !(a == b);
}

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
  explicit ExceptionOr(T&& result)
      : result_{std::move(result)},
        exception_{Exception::kSuccess} {}  // NOLINT
  explicit ExceptionOr(const T& result)
      : result_{result}, exception_{Exception::kSuccess} {}           // NOLINT
  ExceptionOr(Exception::Value exception) : exception_{exception} {}  // NOLINT
  ExceptionOr(Exception exception) : exception_{exception} {}         // NOLINT
  // If there exists explicit conversion from U to T,
  // then allow explicit conversion from ExceptionOr<U> to ExceptionOr<T>.
  template <typename U, typename = absl::void_t<decltype(T{std::declval<U>()})>>
  explicit ExceptionOr<T>(ExceptionOr<U> value) {
    if (!value.ok()) {
      exception_ = value.GetException();
    } else {
      result_ = T{std::move(value.result())};
      exception_ = Exception{Exception::kSuccess};
    }
  }

  bool ok() const { return exception_.value == Exception::kSuccess; }

  T& result() & { return result_; }
  const T& result() const& { return result_; }
  T&& result() && { return std::move(result_); }
  const T&& result() const&& { return std::move(result_); }

  Exception::Value exception() const { return exception_.value; }

  T GetResult() const { return result_; }
  Exception GetException() const { return exception_; }

 private:
  T result_{};
  Exception exception_{Exception::kFailed};
};

template <>
class ExceptionOr<bool> {
 public:
  explicit ExceptionOr() = default;
  explicit ExceptionOr(bool result)
      : exception_{result ? Exception::kSuccess : Exception::kFailed} {}
  explicit ExceptionOr(Exception::Value exception) : exception_{exception} {}
  explicit ExceptionOr(Exception exception) : exception_{exception} {}

  bool ok() const { return exception_.Ok(); }
  explicit operator bool() const { return ok(); }
  bool result() const { return ok(); }
  Exception::Value exception() const { return exception_.value; }
  bool GetResult() const { return ok(); }
  Exception GetException() const { return exception_; }

 private:
  Exception exception_{Exception::kFailed};
};

template <typename T>
constexpr inline bool operator==(const ExceptionOr<T>& a,
                                 const ExceptionOr<T>& b) {
  if (a.ok() && b.ok()) {
    return a.result() == b.result();
  }
  return a.exception() == b.exception();
}

}  // namespace nearby

#endif  // PLATFORM_BASE_EXCEPTION_H_
