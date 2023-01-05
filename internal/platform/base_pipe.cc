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

#include "internal/platform/base_pipe.h"

#include "internal/platform/base_mutex_lock.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {

ExceptionOr<ByteArray> BasePipe::Read(size_t size) {
  BaseMutexLock lock(mutex_.get());

  // We're done reading all the chunks that were written before the OutputStream
  // was closed, so there's nothing to do here other than return an empty chunk
  // to serve as an EOF indication to callers.
  if (read_all_chunks_) {
    return ExceptionOr<ByteArray>{ByteArray{}};
  }

  while (buffer_.empty() && !input_stream_closed_) {
    Exception wait_exception = cond_->Wait();

    if (wait_exception.Raised()) {
      return ExceptionOr<ByteArray>{wait_exception};
    }
  }

  if (input_stream_closed_) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }

  ByteArray first_chunk{buffer_.front()};
  buffer_.pop_front();

  // If we received our sentinel chunk, mark the fact that there cannot
  // possibly be any more chunks to read here on in, and return an empty chunk
  // to serve as an EOF indication to callers.
  if (first_chunk.Empty()) {
    read_all_chunks_ = true;
    return ExceptionOr<ByteArray>{ByteArray{}};
  }

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

Exception BasePipe::Write(const ByteArray& data) {
  BaseMutexLock lock(mutex_.get());

  return WriteLocked(data);
}

void BasePipe::MarkInputStreamClosed() {
  BaseMutexLock lock(mutex_.get());

  input_stream_closed_ = true;
  // Trigger cond_ to unblock a potentially-blocked call to read(), and to let
  // it know to return Exception::IO.
  cond_->Notify();
}

void BasePipe::MarkOutputStreamClosed() {
  BaseMutexLock lock(mutex_.get());

  // Write a sentinel null chunk before marking output_stream_closed as true.
  WriteLocked(ByteArray{});
  output_stream_closed_ = true;
}

Exception BasePipe::WriteLocked(const ByteArray& data) {
  if (input_stream_closed_ || output_stream_closed_) {
    return {Exception::kIo};
  }

  buffer_.push_back(data);
  // Trigger cond_ to unblock a potentially-blocked call to read(), now that
  // there's more data for it to consume.
  cond_->Notify();
  return {Exception::kSuccess};
}

}  // namespace nearby
