#ifndef PLATFORM_EXCEPTION_H_
#define PLATFORM_EXCEPTION_H_

namespace location {
namespace nearby {

struct Exception {
  enum Value {
    NONE,
    IO,
    INTERRUPTED,
    INVALID_PROTOCOL_BUFFER,
    EXECUTION,
  };
};

// ExceptionOr models the concept of the return value of a function that might
// throw an exception.
//
// If ok() returns true, result() is a usable return value. Otherwise,
// exception() explains why such a value is not present.
//
// A typical pattern of usage is as follows:
//
//      if (!e.ok()) {
//        if (Exception::EXCEPTION_TYPE_1 == e.exception()) {
//          // Handle Exception::EXCEPTION_TYPE_1.
//        } else if (Exception::EXCEPTION_TYPE_2 == e.exception()) {
//          // Handle Exception::EXCEPTION_TYPE_2.
//        }
//
//         return;
//      }
//
//      // Use e.result().
template <typename T>
class ExceptionOr {
 public:
  explicit ExceptionOr(T result);
  explicit ExceptionOr(Exception::Value exception);

  bool ok() const;

  T result() const;
  Exception::Value exception() const;

 private:
  T result_;
  Exception::Value exception_;
};

}  // namespace nearby
}  // namespace location

#include "platform/exception.cc"

#endif  // PLATFORM_EXCEPTION_H_
