// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLATFORM_IMPL_LINUX_STREAM_H_
#define PLATFORM_IMPL_LINUX_STREAM_H_

#include <optional>

#include <sdbus-c++/Types.h>

#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace linux {
class InputStream : public nearby::InputStream {
 public:
  explicit InputStream(sdbus::UnixFd fd) : fd_(std::move(fd)){};

  ExceptionOr<ByteArray> Read(std::int64_t size) override;

  Exception Close() override;

 private:
  sdbus::UnixFd fd_;
};

class OutputStream : public nearby::OutputStream {
 public:
  explicit OutputStream(sdbus::UnixFd fd) : fd_(std::move(fd)){};

  Exception Write(const ByteArray &data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  sdbus::UnixFd fd_;
};

}  // namespace linux
}  // namespace nearby

#endif
