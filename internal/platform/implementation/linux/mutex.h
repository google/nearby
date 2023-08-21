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
  explicit Mutex(Mode mode) : mode_(mode) {
    if (mode == Mode::kRecursive)
      mutex_.emplace<std::recursive_mutex>();
    else
      mutex_.emplace<absl::Mutex>();
  }

  ~Mutex() override = default;
  Mutex(Mutex &&) = delete;
  Mutex &operator=(Mutex &&) = delete;
  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override {
    if (auto mutex = std::get_if<absl::Mutex>(&mutex_); mutex != nullptr) {
      if (mode_ == Mode::kRegularNoCheck) {
        mutex->ForgetDeadlockInfo();
      }
      mutex->Lock();
    } else {
      std::get_if<std::recursive_mutex>(&mutex_)->lock();
    }
  }

  void Unlock() ABSL_UNLOCK_FUNCTION() override {
    if (auto mutex = std::get_if<absl::Mutex>(&mutex_); mutex != nullptr) {
      mutex->Unlock();
    } else {
      std::get_if<std::recursive_mutex>(&mutex_)->unlock();
    }
  }

  absl::Mutex *GetRegularMutex() {
    return std::get_if<absl::Mutex>(&mutex_);
  }

private:
  std::variant<absl::Mutex, std::recursive_mutex> mutex_;
  Mode mode_;
};
} // namespace linux
} // namespace nearby
#endif
