#include "platform/base/cancellation_flag.h"

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

  if (cancelled_) {
    // Someone already cancelled. Return immediately.
    return;
  }
  cancelled_ = true;
}

bool CancellationFlag::Cancelled() const {
  absl::MutexLock lock(mutex_.get());

  return cancelled_;
}

}  // namespace nearby
}  // namespace location
