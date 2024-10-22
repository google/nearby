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

#include "internal/platform/blocking_queue_stream.h"

#include <cstdint>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"

namespace nearby {

BlockingQueueStream::BlockingQueueStream() {
  NEARBY_LOGS(INFO) << "Create a BlockingQueueStream with size "
                    << FeatureFlags::GetInstance()
                           .GetFlags()
                           .blocking_queue_stream_queue_capacity;
}

ExceptionOr<ByteArray> BlockingQueueStream::Read(std::int64_t size) {
  if (is_closed_) {
    NEARBY_LOGS(INFO)
        << "Failed to read BlockingQueueStream because it was closed.";
    return ExceptionOr<ByteArray>(Exception::kInterrupted);
  }
  NEARBY_LOGS(INFO) << "BlockingQueueStream expect to read " << size
                    << " bytes";
  return ExceptionOr<ByteArray>(blocking_queue_.Take());
}

void BlockingQueueStream::Write(const ByteArray& bytes) {
  if (is_closed_) {
    NEARBY_LOGS(INFO)
        << "Failed to write BlockingQueueStream because it was closed.";
    return;
  }
  is_writing_ = true;
  blocking_queue_.Put(bytes);
  is_writing_ = false;
  NEARBY_VLOG(1) << "BlockingQueueStream wrote " << bytes.size() << " bytes";
}

Exception BlockingQueueStream::Close() {
  if (is_closed_) {
    NEARBY_LOGS(INFO) << "InputBlockingQueueStream has already been closed.";
    return {Exception::kSuccess};
  }
  if (is_writing_) {
    NEARBY_LOGS(INFO)
        << "BlockingQueueStream is waiting for writing, read first to unblock";
    blocking_queue_.TryTake();
  }
  blocking_queue_.TryPut(queue_end_);
  is_closed_ = true;
  NEARBY_LOGS(INFO) << "InputBlockingQueueStream is closed.";
  return {Exception::kSuccess};
}

}  // namespace nearby
