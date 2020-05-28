#ifndef PLATFORM_V2_IMPL_G3_PIPE_H_
#define PLATFORM_V2_IMPL_G3_PIPE_H_

#include <memory>

#include "platform_v2/base/base_pipe.h"
#include "platform_v2/impl/g3/condition_variable.h"
#include "platform_v2/impl/g3/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

class Pipe : public BasePipe {
 public:
  Pipe() {
    auto mutex = std::make_unique<g3::Mutex>(/*check=*/true);
    auto cond = std::make_unique<g3::ConditionVariable>(mutex.get());
    Setup(std::move(mutex), std::move(cond));
  }
  ~Pipe() override = default;
  Pipe(Pipe &&) = delete;
  Pipe& operator=(Pipe&&) = delete;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_PIPE_H_
