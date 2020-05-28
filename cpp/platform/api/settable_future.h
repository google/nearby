#ifndef PLATFORM_API_SETTABLE_FUTURE_H_
#define PLATFORM_API_SETTABLE_FUTURE_H_

#include "platform/api/platform.h"
#include "platform/api/settable_future_def.h"
#include "platform/exception.h"
#include "platform/ptr.h"
#include "platform/runnable.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {

// "Common" part of implementation.
// Placed here for textual compatibility to minimize scope of changes.
// Can be (and should be) moved to a separate file outside "api" folder.
// TODO(apolyudov): for API v2.0
namespace platform {
namespace impl {

template <typename T>
class SettableFutureImpl : public SettableFuture<T> {
 public:
  SettableFutureImpl() {
    future_ = platform::ImplementationPlatform::createSettableFutureAny();
  }

  ~SettableFutureImpl() override = default;

  bool set(T value) override { return future_->set(absl::any(value)); }

  bool setException(Exception exception) override {
    return future_->setException(exception);
  }

  void addListener(Ptr<Runnable> runnable, Executor* executor) override {
    future_->addListener(runnable, executor);
  }

  ExceptionOr<T> get() override { return CommonGet(future_->get()); }
  ExceptionOr<T> get(std::int64_t timeout_ms) override {
    return CommonGet(future_->get(timeout_ms));
  }

 private:
  ExceptionOr<T> CommonGet(ExceptionOr<absl::any> ret_val) {
    if (ret_val.exception() != Exception::kSuccess) {
      return ExceptionOr<T>{ret_val.exception()};
    }
    return ExceptionOr<T>{absl::any_cast<T>(ret_val.result())};
  }

  Ptr<SettableFuture<absl::any>> future_;
};
}  // namespace impl

template <typename T>
Ptr<SettableFuture<T>> ImplementationPlatform::createSettableFuture() {
  return Ptr<SettableFuture<T>>(new impl::SettableFutureImpl<T>{});
}

}  // namespace platform
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SETTABLE_FUTURE_H_
