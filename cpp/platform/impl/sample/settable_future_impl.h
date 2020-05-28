#ifndef PLATFORM_IMPL_SAMPLE_SETTABLE_FUTURE_IMPL_H_
#define PLATFORM_IMPL_SAMPLE_SETTABLE_FUTURE_IMPL_H_

#include "platform/api/settable_future_def.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace platform {

class SettableFutureImpl : public SettableFuture<absl::any> {
 public:
  explicit SettableFutureImpl() = default;
  ~SettableFutureImpl() override = default;

  bool set(absl::any value) override { return true; }

  bool setException(Exception exception) override { return true; }

  void addListener(Ptr<Runnable> runnable, Executor* executor) override {}

  ExceptionOr<std::any> get() override {
    return ExceptionOr<std::any>{Exception{Exception::kFailed}};
  }

  ExceptionOr<std::any> get(std::int64_t timeout_ms) override {
    return ExceptionOr<std::any>{Exception{Exception::kFailed}};
  }
};

}  // namespace platform
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SAMPLE_SETTABLE_FUTURE_IMPL_H_
