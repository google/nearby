#ifndef PLATFORM_PUBLIC_PIPE_H_
#define PLATFORM_PUBLIC_PIPE_H_

#include "platform/base/base_pipe.h"

namespace location {
namespace nearby {

// See for details:
// http://google3/platform/base/base_pipe.h
class Pipe final : public BasePipe {
 public:
  Pipe();
  ~Pipe() override = default;
  Pipe(Pipe&&) = delete;
  Pipe& operator=(Pipe&&) = delete;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_PIPE_H_
