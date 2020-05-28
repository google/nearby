#ifndef PLATFORM_V2_API_ATOMIC_REFERENCE_H_
#define PLATFORM_V2_API_ATOMIC_REFERENCE_H_

namespace location {
namespace nearby {
namespace api {

// An object reference that may be updated atomically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicReference.html
template <typename T>
class AtomicReference {
 public:
  virtual ~AtomicReference() = default;

  virtual T Get() const & = 0;
  virtual T Get() && = 0;
  virtual void Set(const T& value) = 0;
  virtual void Set(T&& value) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_ATOMIC_REFERENCE_H_
