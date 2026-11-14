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

#ifndef PLATFORM_BASE_CANCELLATION_FLAG_LISTENER_H_
#define PLATFORM_BASE_CANCELLATION_FLAG_LISTENER_H_

#include <utility>

#include "internal/platform/cancellation_flag.h"

namespace nearby {

// An RAII mechanism to register CancelListener over a life cycle of medium
// class.
// Note: A `CancellationFlagListener` must not be destroyed synchronously from
// within its own `CancelListener` callback during `CancellationFlag::Cancel()`,
// and must not be destroyed while holding any lock that its `CancelListener`
// callback acquires.
class CancellationFlagListener {
 public:
  CancellationFlagListener(CancellationFlag* flag,
                           CancellationFlag::CancelListener listener)
      : flag_(flag), listener_(std::move(listener)) {
    flag_->RegisterOnCancelListener(&listener_);
  }

  // Unregisters the listener and blocks if the callback is currently executing
  // on another thread. Must not be invoked synchronously from within
  // `listener_` itself, or while holding any lock acquired by `listener_`.
  ~CancellationFlagListener() { flag_->UnregisterOnCancelListener(&listener_); }

 private:
  CancellationFlag* flag_;
  CancellationFlag::CancelListener listener_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_CANCELLATION_FLAG_LISTENER_H_
