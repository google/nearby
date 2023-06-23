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

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"

namespace nearby {

// A cancellation flag to mark an operation has been cancelled and should be
// cleaned up as soon as possible.
class CancellationFlag {
 public:
  // The listener for cancellation.
  using CancelListener = absl::AnyInvocable<void()>;

  CancellationFlag();
  explicit CancellationFlag(bool cancelled);
  CancellationFlag(const CancellationFlag &) = delete;
  CancellationFlag &operator=(const CancellationFlag &) = delete;
  CancellationFlag(CancellationFlag &&) = default;
  CancellationFlag &operator=(CancellationFlag &&) = default;
  virtual ~CancellationFlag();

  // Set the flag as cancelled.
  void Cancel() ABSL_LOCKS_EXCLUDED(mutex_);

  // Set the flag as uncancelled. This is needed for the case where the same
  // endpoint is being reused, and callers want to reset the cancellation flag
  // for the new attempt. Without this API, the reused endpoint will
  // use the cancelled flag.
  //
  // It is expected that calleers will only call `Uncancel()` if the flag is
  // already cancelled (check via `Cancelled()`).
  void Uncancel() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if the flag has been set to cancelled.
  bool Cancelled() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class CancellationFlagListener;
  friend class CancellationFlagPeer;

  // The registration inserts the pointer of caller's listener callback into
  // `listeners_`, a flat hash set which support the pointer type for hashing
  // function. It conducts that 2 different pointers might point to the same
  // callback function which is unusual and should avoid. Hence we make it as
  // private and use `CancellationFlagListener` as a RAII to wrap the function.
  // The caller should register listener as lambda or absl::AnyInvocable
  // via `CancellationFlagListener`.
  void RegisterOnCancelListener(CancelListener *listener)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // The un-registration erases the pointer of caller's listener callback from
  // `listeners_`. This is paired to RegisterOnCancelListener which is
  // guaranteed to be called under `CancellationFlagListener`.
  void UnregisterOnCancelListener(CancelListener *listener)
      ABSL_LOCKS_EXCLUDED(mutex_);

  int CancelListenersSize() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_.get());
    return listeners_.size();
  }

  std::unique_ptr<absl::Mutex> mutex_;
  bool cancelled_ ABSL_GUARDED_BY(mutex_) = false;
  absl::flat_hash_set<CancelListener *> ABSL_GUARDED_BY(mutex_) listeners_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_CANCELLATION_FLAG_H_
