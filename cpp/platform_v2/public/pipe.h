#ifndef PLATFORM_V2_PUBLIC_PIPE_H_
#define PLATFORM_V2_PUBLIC_PIPE_H_

#include "platform_v2/base/base_pipe.h"

namespace location {
namespace nearby {

// See for details:
// http://google3/platform_v2/base/base_pipe.h
class Pipe final : public BasePipe {
 public:
  Pipe();
  ~Pipe() override = default;
  Pipe(Pipe&&) = delete;
  Pipe& operator=(Pipe&&) = delete;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_PIPE_H_
