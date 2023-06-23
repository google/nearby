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

#include "internal/platform/feature_flags.h"

namespace nearby {

CancellationFlag::CancellationFlag() {
  mutex_ = std::make_unique<absl::Mutex>();
}

CancellationFlag::CancellationFlag(bool cancelled) {
  mutex_ = std::make_unique<absl::Mutex>();
  cancelled_ = cancelled;
}

CancellationFlag::~CancellationFlag() {
  absl::MutexLock lock(mutex_.get());
  listeners_.clear();
}

void CancellationFlag::Cancel() {
  // Return immediately as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return;
  }

  absl::flat_hash_set<CancelListener *> listeners;
  {
    absl::MutexLock lock(mutex_.get());
    if (cancelled_) {
      // Someone already cancelled. Return immediately.
      return;
    }
    cancelled_ = true;

    listeners = listeners_;
  }

  for (auto *listener : listeners) {
    (*listener)();
  }
}

void CancellationFlag::Uncancel() {
  // Return immediately as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return;
  }

  {
    absl::MutexLock lock(mutex_.get());
    assert(cancelled_);
    cancelled_ = false;
  }
}

bool CancellationFlag::Cancelled() const {
  absl::MutexLock lock(mutex_.get());

  // Return false as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return false;
  }

  return cancelled_;
}

void CancellationFlag::RegisterOnCancelListener(CancelListener *listener) {
  absl::MutexLock lock(mutex_.get());

  listeners_.emplace(listener);
}

void CancellationFlag::UnregisterOnCancelListener(CancelListener *listener) {
  absl::MutexLock lock(mutex_.get());

  listeners_.erase(listener);
}

}  // namespace nearby
