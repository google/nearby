#ifndef PLATFORM_V2_IMPL_G3_MUTEX_H_
#define PLATFORM_V2_IMPL_G3_MUTEX_H_

#include "platform_v2/api/mutex.h"
#include "platform_v2/impl/shared/posix_mutex.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

class ABSL_LOCKABLE Mutex : public api::Mutex {
 public:
  explicit Mutex(bool check) : check_(check) {}
  ~Mutex() override = default;
  Mutex(Mutex&&) = delete;
  Mutex& operator=(Mutex&&) = delete;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override {
    mutex_.Lock();
    if (!check_) mutex_.ForgetDeadlockInfo();
  }
  void Unlock() ABSL_UNLOCK_FUNCTION() override { mutex_.Unlock(); }

 private:
  friend class ConditionVariable;
  absl::Mutex mutex_;
  bool check_;
};

class ABSL_LOCKABLE RecursiveMutex : public posix::Mutex {
 public:
  ~RecursiveMutex() override = default;
  RecursiveMutex() = default;
  RecursiveMutex(RecursiveMutex&&) = delete;
  RecursiveMutex& operator=(RecursiveMutex&&) = delete;
  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_MUTEX_H_
