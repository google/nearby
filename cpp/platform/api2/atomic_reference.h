#ifndef PLATFORM_API2_ATOMIC_REFERENCE_H_
#define PLATFORM_API2_ATOMIC_REFERENCE_H_

namespace location {
namespace nearby {

// An object reference that may be updated atomically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicReference.html
template <typename T>
class AtomicReference {
 public:
  virtual ~AtomicReference() {}

  virtual T Get() = 0;
  virtual void Set(const T& value) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_ATOMIC_REFERENCE_H_
