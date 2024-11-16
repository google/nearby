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

#ifndef PLATFORM_BASE_EXPECTED_H_
#define PLATFORM_BASE_EXPECTED_H_

#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "proto/connections_enums.pb.h"

namespace nearby {

class Error {
 public:
  Error() = default;

  explicit Error(location::nearby::proto::connections::OperationResultCode
                     operation_result_code)
      : operation_result_code_(operation_result_code) {}

  bool success() const {
    return operation_result_code_ == location::nearby::proto::connections::
                                         OperationResultCode::DETAIL_SUCCESS;
  }
  bool failure() const { return !success(); }

  std::optional<location::nearby::proto::connections::OperationResultCode>
  operation_result_code() const {
    return operation_result_code_;
  }

  static Error Success() { return Error(); }

 private:
  location::nearby::proto::connections::OperationResultCode
      operation_result_code_ = location::nearby::proto::connections::
          OperationResultCode::DETAIL_UNKNOWN;
};

// Forward declare.
template <typename E>
class Unexpected;

template <typename T, typename E>
class Expected {
 public:
  constexpr Expected(T value) : data_(std::move(value)) {}  // NOLINT
  constexpr Expected(Unexpected<E> u);                      // NOLINT

  constexpr operator bool() const {  // NOLINT
    return has_value();
  }

  constexpr T& operator*() & { return value(); }
  constexpr const T& operator*() const& { return value(); }
  constexpr T&& operator*() && { return std::move(value()); }
  constexpr const T& operator*() const&& { return std::move(value()); }

  constexpr T* operator->() { return &value(); }
  constexpr const T* operator->() const { return &value(); }

  constexpr bool has_value() const { return std::holds_alternative<T>(data_); }
  constexpr bool has_error() const { return std::holds_alternative<E>(data_); }

  constexpr T& value() & { return std::get<T>(data_); }
  constexpr const T& value() const& { return std::get<T>(data_); }
  constexpr T&& value() && { return std::get<T>(std::move(data_)); }
  constexpr const T& value() const&& { return std::get<T>(std::move(data_)); }

  constexpr E& error() & { return std::get<E>(data_); }
  constexpr const E& error() const& { return std::get<E>(data_); }
  constexpr E&& error() && { return std::get<E>(std::move(data_)); }
  constexpr const E&& error() const&& { return std::get<E>(std::move(data_)); }

 private:
  std::variant<T, E> data_;
};

template <typename E>
class Unexpected {
 public:
  constexpr Unexpected(E error) : error_(std::move(error)) {}  // NOLINT

 private:
  template <typename, typename>
  friend class Expected;

  E error_;
};

Unexpected(const char*) -> Unexpected<std::string>;

template <typename T, typename E>
constexpr Expected<T, E>::Expected(Unexpected<E> u)
    : data_(std::move(u.error_)) {}

template <typename T>
class ErrorOr : public Expected<T, Error> {
 public:
  using Expected<T, Error>::Expected;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_EXPECTED_H_
