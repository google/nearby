// Copyright 2024 Google LLC
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

#ifndef PLATFORM_PUBLIC_BLOCKING_QUEUE_STREAM_H_
#define PLATFORM_PUBLIC_BLOCKING_QUEUE_STREAM_H_

#include <cstdint>

#include "internal/platform/array_blocking_queue.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"

namespace nearby {
class BlockingQueueStream : public InputStream {
 public:
  BlockingQueueStream();
  ~BlockingQueueStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  void Write(const ByteArray& bytes);
  Exception Close() override;
  bool IsWriting() const {
    return is_writing_;
  }

 private:
  mutable Mutex mutex_;
  ArrayBlockingQueue<ByteArray> blocking_queue_{FeatureFlags::GetInstance()
                 .GetFlags()
                 .blocking_queue_stream_queue_capacity};
  ByteArray queue_end_{0};
  bool is_writing_ = false;
  bool is_closed_ = false;
};
}  // namespace nearby

#endif  // #ifndef PLATFORM_PUBLIC_BLOCKING_QUEUE_STREAM_H_

