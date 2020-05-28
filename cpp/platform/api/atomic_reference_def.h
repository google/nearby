#ifndef PLATFORM_API_ATOMIC_REFERENCE_DEF_H_
#define PLATFORM_API_ATOMIC_REFERENCE_DEF_H_

namespace location {
namespace nearby {

// An object reference that may be updated atomically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicReference.html
//
// Platform must implentent non-template static member functions
//   Ptr<AtomicReference<size_t>> CreateAtomicReferenceSizeT()
//   Ptr<AtomicReference<std::shared_ptr<void>>> CreateAtomicReferencePtr()
// in the location::nearby::platform::ImplementationPlatform class.
template <typename T>
class AtomicReference {
 public:
  virtual ~AtomicReference() = default;

  virtual T get() = 0;
  virtual void set(T value) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_ATOMIC_REFERENCE_DEF_H_
