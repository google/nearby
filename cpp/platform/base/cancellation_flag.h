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
