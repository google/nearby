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

#ifndef PLATFORM_BASE_CANCELLATION_FLAG_H_
#define PLATFORM_BASE_CANCELLATION_FLAG_H_

#include <memory>

#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {

// A cancellation flag to mark an operation has been cancelled and should be
// cleaned up as soon as possible.
class CancellationFlag {
 public:
  CancellationFlag();
  explicit CancellationFlag(bool cancelled);
  CancellationFlag(const CancellationFlag &) = delete;
  CancellationFlag &operator=(const CancellationFlag &) = delete;
  CancellationFlag(CancellationFlag &&) = default;
  CancellationFlag &operator=(CancellationFlag &&) = default;
  virtual ~CancellationFlag() = default;

  // Set the flag as cancelled.
  void Cancel() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if the flag has been set to cancelled.
  bool Cancelled() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  std::unique_ptr<absl::Mutex> mutex_;
  bool cancelled_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_CANCELLATION_FLAG_H_
