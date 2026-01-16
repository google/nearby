#ifndef PLATFORM_IMPL_LINUX_MUTEX_H_
#define PLATFORM_IMPL_LINUX_MUTEX_H_

#include <mutex>
#include <variant>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/mutex.h"

namespace nearby {
namespace linux {
class ABSL_LOCKABLE Mutex : public api::Mutex {
 public:
  explicit Mutex(Mode mode) : mode_(mode) {}
  ~Mutex() override = default;
  Mutex(Mutex&&) = delete;
  Mutex& operator=(Mutex&&) = delete;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override {
    if (mode_ == Mode::kRegularNoCheck) mutex_.ForgetDeadlockInfo();
    if (mode_ == Mode::kRegular || mode_ == Mode::kRegularNoCheck) {
      mutex_.Lock();
    } else {
      recursive_mutex_.lock();
    }
  }

  void Unlock() ABSL_UNLOCK_FUNCTION() override {
    if (mode_ == Mode::kRegular || mode_ == Mode::kRegularNoCheck) {
      mutex_.Unlock();
    } else {
      recursive_mutex_.unlock();
    }
  }

  absl::Mutex& GetMutex() { return mutex_; }
  std::recursive_mutex& GetRecursiveMutex() { return recursive_mutex_; }

 private:
  friend class ConditionVariable;
  absl::Mutex mutex_;
  std::recursive_mutex recursive_mutex_;  //  The actual mutex allocation
  Mode mode_;
};
}  // namespace linux
}  // namespace nearby
#endif
