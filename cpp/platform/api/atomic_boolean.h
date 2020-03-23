#ifndef PLATFORM_API_ATOMIC_BOOLEAN_H_
#define PLATFORM_API_ATOMIC_BOOLEAN_H_

namespace location {
namespace nearby {

// A boolean value that may be updated atomically.
//
// https://docs.oracle.com/javase/7/docs/api/java/util/concurrent/atomic/AtomicBoolean.html
class AtomicBoolean {
 public:
  virtual ~AtomicBoolean() {}

  virtual bool get() = 0;
  virtual void set(bool value) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_ATOMIC_BOOLEAN_H_
