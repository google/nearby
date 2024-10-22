// Copyright 2020 Google LLC
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

#include "internal/platform/pipe.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/output_stream.h"

namespace nearby {

namespace {
class Pipe {
 public:
  Pipe() = default;

  class PipeInputStream : public InputStream {
   public:
    explicit PipeInputStream(std::shared_ptr<Pipe> pipe) : pipe_(pipe) {}
    ~PipeInputStream() override { DoClose(); }

    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return pipe_->Read(size);
    }
    Exception Close() override { return DoClose(); }

   private:
    Exception DoClose() {
      pipe_->MarkInputStreamClosed();
      return {Exception::kSuccess};
    }
    std::shared_ptr<Pipe> pipe_;
  };

  class PipeOutputStream : public OutputStream {
   public:
    explicit PipeOutputStream(std::shared_ptr<Pipe> pipe) : pipe_(pipe) {}
    ~PipeOutputStream() override { DoClose(); }

    Exception Write(const ByteArray& data) override {
      return pipe_->Write(data);
    }
    Exception Flush() override { return {Exception::kSuccess}; }
    Exception Close() override { return DoClose(); }

   private:
    Exception DoClose() {
      pipe_->MarkOutputStreamClosed();
      return {Exception::kSuccess};
    }
    std::shared_ptr<Pipe> pipe_;
  };

 private:
  ExceptionOr<ByteArray> Read(size_t size) ABSL_LOCKS_EXCLUDED(mutex_);
  Exception Write(const ByteArray& data) ABSL_LOCKS_EXCLUDED(mutex_);

  void MarkInputStreamClosed() ABSL_LOCKS_EXCLUDED(mutex_);
  void MarkOutputStreamClosed() ABSL_LOCKS_EXCLUDED(mutex_);

  Exception WriteLocked(const ByteArray& data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool input_stream_closed_ ABSL_GUARDED_BY(mutex_) = false;
  bool output_stream_closed_ ABSL_GUARDED_BY(mutex_) = false;
  bool read_all_chunks_ ABSL_GUARDED_BY(mutex_) = false;

  std::deque<ByteArray> ABSL_GUARDED_BY(mutex_) buffer_;
  // Order of declaration matters:
  // - mutex must be defined before condvar;
  Mutex mutex_;
  ConditionVariable cond_{&mutex_};
};

ExceptionOr<ByteArray> Pipe::Read(size_t size) {
  MutexLock lock(&mutex_);

  // We're done reading all the chunks that were written before the OutputStream
  // was closed, so there's nothing to do here other than return an empty chunk
  // to serve as an EOF indication to callers.
  if (read_all_chunks_) {
    return ExceptionOr<ByteArray>{ByteArray{}};
  }

  while (buffer_.empty() && !input_stream_closed_) {
    Exception wait_exception = cond_.Wait();

    if (wait_exception.Raised()) {
      return ExceptionOr<ByteArray>{wait_exception};
    }
  }

  // If we received our sentinel chunk, mark the fact that there cannot
  // possibly be any more chunks to read here on in, and return an empty chunk
  // to serve as an EOF indication to callers.
  if (buffer_.empty() || buffer_.front().Empty()) {
    read_all_chunks_ = true;
    return ExceptionOr<ByteArray>{ByteArray{}};
  }

  ByteArray first_chunk{buffer_.front()};
  buffer_.pop_front();

  // If first_chunk is small enough to not overshoot the requested 'size', just
  // return that.
  if (first_chunk.size() <= size) {
    return ExceptionOr<ByteArray>{first_chunk};
  } else {
    // Break first_chunk into 2 parts -- the first one of which (next_chunk)
    // will be 'size' bytes long, and will be returned, and the second one of
    // which (overflow_chunk) will be re-inserted into buffer_, at the head of
    // the queue, to be served up in the next call to read().
    ByteArray next_chunk(first_chunk.data(), size);
    buffer_.push_front(
        ByteArray(first_chunk.data() + size, first_chunk.size() - size));
    return ExceptionOr<ByteArray>{next_chunk};
  }
}

Exception Pipe::Write(const ByteArray& data) {
  MutexLock lock(&mutex_);

  return WriteLocked(data);
}

void Pipe::MarkInputStreamClosed() {
  MutexLock lock(&mutex_);
  if (input_stream_closed_) return;
  input_stream_closed_ = true;
  // Trigger cond_ to unblock a potentially-blocked call to read(), and to let
  // it know to return Exception::IO.
  cond_.Notify();
}

void Pipe::MarkOutputStreamClosed() {
  MutexLock lock(&mutex_);
  if (output_stream_closed_) return;
  // Write a sentinel null chunk before marking output_stream_closed as true.
  WriteLocked(ByteArray{});
  output_stream_closed_ = true;
}

Exception Pipe::WriteLocked(const ByteArray& data) {
  if (input_stream_closed_ || output_stream_closed_) {
    return {Exception::kIo};
  }

  buffer_.push_back(data);
  // Trigger cond_ to unblock a potentially-blocked call to read(), now that
  // there's more data for it to consume.
  cond_.Notify();
  return {Exception::kSuccess};
}

}  // namespace

std::pair<std::unique_ptr<InputStream>, std::unique_ptr<OutputStream>>
CreatePipe() {
  auto pipe = std::make_shared<Pipe>();
  return std::make_pair(std::make_unique<Pipe::PipeInputStream>(pipe),
                        std::make_unique<Pipe::PipeOutputStream>(pipe));
}
}  // namespace nearby
