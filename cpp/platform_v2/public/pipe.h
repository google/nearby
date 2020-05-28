#ifndef PLATFORM_V2_PUBLIC_PIPE_H_
#define PLATFORM_V2_PUBLIC_PIPE_H_

#include "platform_v2/base/base_pipe.h"

namespace location {
namespace nearby {

// See for details:
// TODO(apolyudov): replace with cs/ link once it becomes available.
// https://critique-ng.corp.google.com/cl/310492721/depot/google3/platform_v2/base/base_pipe.h
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
