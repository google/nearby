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

#include "internal/platform/cancellation_flag.h"

#include <cassert>
#include <memory>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/feature_flags.h"

namespace nearby {
namespace {

// Tracks the CancellationFlag instances currently executing their Cancel() loop
// on the current thread so re-entrant Cancel() calls from within a listener
// callback do not deadlock waiting on the outer Cancel() frame.
class ActiveCancelScope {
 public:
  explicit ActiveCancelScope(const CancellationFlag* flag)
      : flag_(flag), parent_(active_cancel_scope_) {
    active_cancel_scope_ = this;
  }

  ~ActiveCancelScope() { active_cancel_scope_ = parent_; }

  static bool IsCancellingOnCurrentThread(const CancellationFlag* flag) {
    for (const ActiveCancelScope* scope = active_cancel_scope_;
         scope != nullptr; scope = scope->parent_) {
      if (scope->flag_ == flag) {
        return true;
      }
    }
    return false;
  }

 private:
  const CancellationFlag* const flag_;
  const ActiveCancelScope* const parent_;
  ABSL_CONST_INIT static thread_local const ActiveCancelScope*
      active_cancel_scope_;
};

ABSL_CONST_INIT thread_local const ActiveCancelScope*
    ActiveCancelScope::active_cancel_scope_ = nullptr;

}  // namespace

CancellationFlag::CancellationFlag() {
  mutex_ = std::make_unique<absl::Mutex>();
}

CancellationFlag::CancellationFlag(bool cancelled) {
  mutex_ = std::make_unique<absl::Mutex>();
  cancelled_ = cancelled;
}

CancellationFlag::~CancellationFlag() {
  absl::MutexLock lock(*mutex_);
  pending_cancel_listeners_.clear();
  listeners_.clear();
}

void CancellationFlag::Cancel() {
  // Return immediately as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return;
  }

  {
    absl::MutexLock lock(*mutex_);
    const bool already_cancelled = cancelled_;
    if (!already_cancelled) {
      cancelled_ = true;
      pending_cancel_listeners_ = listeners_;
    }
    if (in_cancel_) {
      if (!ActiveCancelScope::IsCancellingOnCurrentThread(this)) {
        auto is_not_in_cancel = [this]() ABSL_SHARED_LOCKS_REQUIRED(*mutex_) {
          return !in_cancel_;
        };
        mutex_->Await(absl::Condition(&is_not_in_cancel));
      }
      return;
    }
    if (already_cancelled) {
      return;
    }
    in_cancel_ = true;
  }

  // Invoke listeners outside the lock so callbacks can query Cancelled(),
  // re-enter Cancel() (even after Uncancel()), or acquire external locks
  // without lock-order inversion. Track `active_listener_` so
  // UnregisterOnCancelListener() on another thread waits if that specific
  // listener is currently executing.
  ActiveCancelScope cancel_scope(this);
  while (true) {
    CancelListener* listener = nullptr;
    {
      absl::MutexLock lock(*mutex_);
      active_listener_ = nullptr;
      if (!cancelled_ || pending_cancel_listeners_.empty()) {
        in_cancel_ = false;
        break;
      }
      auto it = pending_cancel_listeners_.begin();
      listener = *it;
      pending_cancel_listeners_.erase(it);
      active_listener_ = listener;
    }
    (*listener)();
  }
}

void CancellationFlag::Uncancel() {
  // Return immediately as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return;
  }

  {
    absl::MutexLock lock(*mutex_);
    assert(cancelled_);
    cancelled_ = false;
    pending_cancel_listeners_.clear();
  }
}

bool CancellationFlag::Cancelled() const {
  absl::MutexLock lock(*mutex_);

  // Return false as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return false;
  }

  return cancelled_;
}

void CancellationFlag::RegisterOnCancelListener(CancelListener *listener) {
  absl::MutexLock lock(*mutex_);

  listeners_.emplace(listener);
}

void CancellationFlag::UnregisterOnCancelListener(CancelListener *listener) {
  if (listener == nullptr) {
    return;
  }
  absl::MutexLock lock(*mutex_);

  listeners_.erase(listener);
  pending_cancel_listeners_.erase(listener);
  // If `listener` is currently executing on another thread inside `Cancel()`,
  // wait for it to finish before returning so `~CancellationFlagListener()`
  // does not destroy `listener_` while `(*listener)()` is still running.
  // A listener callback must not destroy its own `CancellationFlagListener`
  // synchronously from inside its own invocation, as that would destroy the
  // executing `absl::AnyInvocable` while still on the call stack.
  if (active_listener_ == listener) {
    auto is_listener_inactive =
        [this, listener]() ABSL_SHARED_LOCKS_REQUIRED(*mutex_) {
          return active_listener_ != listener;
        };
    mutex_->Await(absl::Condition(&is_listener_inactive));
  }
}

}  // namespace nearby
