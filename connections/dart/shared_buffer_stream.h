// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_DART_SHARED_BUFFER_STREAM_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_DART_SHARED_BUFFER_STREAM_H_

#include <stdint.h>

#include <cstddef>
#include <map>
#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "connections/dart/shared_buffer_manager.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"

static int32_t g_shared_buffer_size = 1024;
// Global shared buffer structure
struct SharedBuffer {
  uint8_t* buffer ABSL_GUARDED_BY(mutex);
  size_t bufferSize ABSL_GUARDED_BY(mutex);
  size_t writePosition ABSL_GUARDED_BY(mutex);
  size_t readPosition ABSL_GUARDED_BY(mutex);
  bool isForeRunner ABSL_GUARDED_BY(mutex);
  bool dataAvailable ABSL_GUARDED_BY(mutex);
  bool stopRequested ABSL_GUARDED_BY(mutex);
  bool stopWriting ABSL_GUARDED_BY(mutex);
  absl::Mutex mutex;
  absl::CondVar condition ABSL_GUARDED_BY(mutex);
  absl::Condition data_or_stop_condition ABSL_GUARDED_BY(mutex);
  int64_t payload_id;
  void* instance;
  std::string endpoint_id;
  void (*result_cb)(int64_t payload_id, int32_t result);
  SharedBuffer()
      : data_or_stop_condition(
            +[](SharedBuffer* sb) ABSL_SHARED_LOCKS_REQUIRED(mutex) {
              return sb->dataAvailable || sb->stopRequested;
            },
            this) {}
};

class SharedBufferStream : public nearby::InputStream {
 public:
  SharedBufferStream(SharedBuffer* shared_buffer,
                     SharedBufferManager* shared_buffer_manager,
                     void* instance);
  ~SharedBufferStream() override;

  nearby::ExceptionOr<nearby::ByteArray> Read(std::int64_t size) override
      ABSL_LOCKS_EXCLUDED(shared_buffer_->mutex);
  ;
  nearby::Exception Close() override ABSL_LOCKS_EXCLUDED(shared_buffer_->mutex);
  ;
  bool IsClosed();
  void StopReading();

  // bool join_thread_ ABSL_GUARDED_BY(mutex_join_) = false;
  // absl::Mutex mutex_join_;

 private:
  SharedBuffer* shared_buffer_;
  SharedBufferManager* shared_buffer_manager_;
  void* instance_;
  // std::thread reader_thread_;
  bool is_closed_ = false;
};

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_DART_SHARED_BUFFER_STREAM_H_
