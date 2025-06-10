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

#include "connections/c/core/shared_buffer_stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>  // NOLINT(build/c++11)

#include "absl/synchronization/mutex.h"
#include "connections/c/core/shared_buffer_manager.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"

SharedBufferStream::SharedBufferStream(
    SharedBuffer *shared_buffer, SharedBufferManager *shared_buffer_manager,
    void *instance)
    : shared_buffer_(shared_buffer),
      shared_buffer_manager_(shared_buffer_manager),
      instance_(instance) {
  shared_buffer_->instance = instance_;
}

SharedBufferStream::~SharedBufferStream() {
  if (!is_closed_) {
    auto result = Close();
    if (result.Ok() == false) {
      std::cerr << "Failed to close shared buffer stream: " << result.value;
    }
  }
  if (reader_thread_.joinable()) {
    reader_thread_.join();
  }
}

nearby::ExceptionOr<nearby::ByteArray> SharedBufferStream::Read(
    std::int64_t size) {
  {
    absl::MutexLock lock(&mutex_join_);
    if (join_thread_) {
      return nearby::ExceptionOr<nearby::ByteArray>(
          nearby::Exception(nearby::Exception::kIo));
    }
  }
  if (is_closed_) {
    return nearby::ExceptionOr<nearby::ByteArray>(
        nearby::Exception(nearby::Exception::kIo));
  }
  if (shared_buffer_ == nullptr) {
    return nearby::ExceptionOr<nearby::ByteArray>(
        nearby::Exception(nearby::Exception::kIo));
  }
  absl::MutexLock lock(&shared_buffer_->mutex);

  while (!shared_buffer_->dataAvailable && !shared_buffer_->stopRequested) {
    shared_buffer_->condition.Wait(&shared_buffer_->mutex);
  }

  if (shared_buffer_->stopRequested) {
    return nearby::ExceptionOr<nearby::ByteArray>(
        nearby::Exception(nearby::Exception::kIo));
  }

  size_t bytes_to_read =
      std::min(static_cast<size_t>(size),
               shared_buffer_->writePosition - shared_buffer_->readPosition);
  if (bytes_to_read <= 0) {
    return nearby::ExceptionOr<nearby::ByteArray>(
        nearby::Exception(nearby::Exception::kIo));
  }
  nearby::ByteArray result(
      reinterpret_cast<const char *>(shared_buffer_->buffer +
                                     shared_buffer_->readPosition),
      bytes_to_read);
  shared_buffer_->readPosition += bytes_to_read;
  if (shared_buffer_->readPosition >= shared_buffer_->writePosition) {
    shared_buffer_->dataAvailable = false;
    shared_buffer_->readPosition = 0;
    shared_buffer_->writePosition = 0;
    shared_buffer_->condition.Signal();
  }

  return nearby::ExceptionOr<nearby::ByteArray>(result);
}

nearby::Exception SharedBufferStream::Close() {
  if (is_closed_) {
    return nearby::Exception(nearby::Exception::kSuccess);
  }
  if (shared_buffer_ == nullptr) {
    return nearby::Exception(nearby::Exception::kIo);
  }
  {
    absl::MutexLock lock(&mutex_join_);
    join_thread_ = true;
  }
  {
    absl::MutexLock lock(&shared_buffer_->mutex);
    LOG(INFO) << "StopRequested set as true in Close";
    shared_buffer_->stopRequested = true;
    shared_buffer_->condition.Signal();
  }
  if (reader_thread_.joinable()) {
    reader_thread_.join();
  }
  is_closed_ = true;

  // Clear the shared buffer from the map.
  shared_buffer_manager_->EraseSharedBuffer(shared_buffer_->payload_id);
  return nearby::Exception(nearby::Exception::kSuccess);
}

bool SharedBufferStream::IsClosed() { return is_closed_; }
void SharedBufferStream::StopReading() {
  if (reader_thread_.joinable()) {
    reader_thread_.join();
  }
}
