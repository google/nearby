#include "platform/exception.h"

namespace location {
namespace nearby {

template <typename T>
ExceptionOr<T>::ExceptionOr(T result)
    : result_(result), exception_(Exception::NONE) {}

template <typename T>
ExceptionOr<T>::ExceptionOr(Exception::Value exception)
    : result_(), exception_(exception) {}

template <typename T>
bool ExceptionOr<T>::ok() const {
  return Exception::NONE == exception_;
}

template <typename T>
T ExceptionOr<T>::result() const {
  return result_;
}

template <typename T>
Exception::Value ExceptionOr<T>::exception() const {
  return exception_;
}

}  // namespace nearby
}  // namespace location
