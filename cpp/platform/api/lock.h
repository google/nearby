#ifndef PLATFORM_API_LOCK_H_
#define PLATFORM_API_LOCK_H_

namespace location {
namespace nearby {

// A lock is a tool for controlling access to a shared resource by multiple
// threads.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class Lock {
 public:
  virtual ~Lock() {}

  virtual void lock() = 0;
  virtual void unlock() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_LOCK_H_
