#ifndef PLATFORM_V2_API_CANCELABLE_H_
#define PLATFORM_V2_API_CANCELABLE_H_

namespace location {
namespace nearby {
namespace api {

// An interface to provide a cancellation mechanism for objects that represent
// long-running operations.
class Cancelable {
 public:
  virtual ~Cancelable() = default;

  virtual bool Cancel() = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_CANCELABLE_H_
