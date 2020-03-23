#ifndef PLATFORM_CANCELABLE_H_
#define PLATFORM_CANCELABLE_H_

namespace location {
namespace nearby {

// An interface to provide a cancellation mechanism for objects that represent
// long-running operations.
class Cancelable {
 public:
  virtual ~Cancelable() {}

  virtual bool cancel() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_CANCELABLE_H_
