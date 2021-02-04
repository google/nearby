#include "platform/base/cancellation_flag.h"

#include "platform/base/feature_flags.h"

namespace location {
namespace nearby {

CancellationFlag::CancellationFlag() {
  mutex_ = std::make_unique<absl::Mutex>();
}

CancellationFlag::CancellationFlag(bool cancelled) {
  mutex_ = std::make_unique<absl::Mutex>();
  cancelled_ = cancelled;
}

void CancellationFlag::Cancel() {
  absl::MutexLock lock(mutex_.get());

  // Return immediately as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return;
  }

  if (cancelled_) {
    // Someone already cancelled. Return immediately.
    return;
  }
  cancelled_ = true;
}

bool CancellationFlag::Cancelled() const {
  absl::MutexLock lock(mutex_.get());

  // Return falsea as no-op if feature flag is not enabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return false;
  }

  return cancelled_;
}

}  // namespace nearby
}  // namespace location
