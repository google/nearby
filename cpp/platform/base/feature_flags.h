#ifndef PLATFORM_BASE_FEATURE_FLAGS_H_
#define PLATFORM_BASE_FEATURE_FLAGS_H_

#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {

// Global flags that control feature availability. This may be used to gating
// features in development, QA testing and releasing.
class FeatureFlags {
 public:
  // Holds for all the feature flags.
  struct Flags {
    bool enable_cancellation_flag = false;
    bool resume_before_disconnect = true;
    // Disable ServiceController API (using StoppableServiceController) when
    // ServiceController is released to prevent calls to that API from
    // other threads.
    bool disable_released_service_controller = true;
  };

  static const FeatureFlags& GetInstance() {
    static const FeatureFlags* instance = new FeatureFlags();
    return *instance;
  }

  const Flags& GetFlags() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::ReaderMutexLock lock(&mutex_);
    return flags_;
  }

 private:
  FeatureFlags() = default;

  // MediumEnvironment is testing uitl class. Use friend class here to enable
  // SetFlags for feature controlling need in test environment.
  friend class MediumEnvironment;
  void SetFlags(const Flags& flags) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    flags_ = flags;
  }
  Flags flags_ ABSL_GUARDED_BY(mutex_);
  mutable absl::Mutex mutex_;
};
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_FEATURE_FLAGS_H_
