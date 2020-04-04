#ifndef PLATFORM_API2_MUTEX_H_
#define PLATFORM_API2_MUTEX_H_

namespace location {
namespace nearby {

// A lock is a tool for controlling access to a shared resource by multiple
// threads.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class Mutex {
 public:
  virtual ~Mutex() {}

  virtual void Lock() = 0;
  virtual void Unlock() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_MUTEX_H_
