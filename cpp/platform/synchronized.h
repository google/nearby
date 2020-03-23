#ifndef PLATFORM_SYNCHRONIZED_H_
#define PLATFORM_SYNCHRONIZED_H_

#include "platform/api/lock.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// An RAII mechanism to acquire a Lock over a block of code.
//
// https://docs.oracle.com/javase/tutorial/essential/concurrency/syncmeth.html
// https://docs.oracle.com/javase/tutorial/essential/concurrency/locksync.html
class Synchronized {
 public:
  explicit Synchronized(Ptr<Lock> lock) : lock_(lock) { lock_->lock(); }
  ~Synchronized() { lock_->unlock(); }

 private:
  Ptr<Lock> lock_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_SYNCHRONIZED_H_
