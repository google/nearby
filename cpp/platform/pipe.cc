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

#include "platform/pipe.h"

#include "platform/api/platform.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {

namespace {
using Platform = platform::ImplementationPlatform;
}

namespace pipe {

class PipeInputStream : public InputStream {
 public:
  explicit PipeInputStream(Ptr<Pipe> pipe) : pipe_(pipe) {}
  ~PipeInputStream() override { close(); }
  ExceptionOr<ConstPtr<ByteArray>> read() override { return read(kChunkSize); }

  ExceptionOr<ConstPtr<ByteArray>> read(std::int64_t size) override {
    return pipe_->read(size);
  }

  Exception::Value close() override {
    pipe_->markInputStreamClosed();

    return Exception::NONE;
  }

 private:
  static constexpr std::int64_t kChunkSize = 64 * 1024;

  Ptr<Pipe> pipe_;
};

class PipeOutputStream : public OutputStream {
 public:
  explicit PipeOutputStream(Ptr<Pipe> pipe) : pipe_(pipe) {}
  ~PipeOutputStream() override { close(); }

  Exception::Value write(ConstPtr<ByteArray> data) override {
    // Avoid leaks.
    ScopedPtr<ConstPtr<ByteArray>> scoped_data(data);

    return pipe_->write(scoped_data.release());
  }

  Exception::Value flush() override {
    // No-op.
    return Exception::NONE;
  }

  Exception::Value close() override {
    pipe_->markOutputStreamClosed();

    return Exception::NONE;
  }

 private:
  Ptr<Pipe> pipe_;
};

}  // namespace pipe

Pipe::Pipe()
    : lock_(Platform::createLock()),
      cond_(Platform::createConditionVariable(lock_.get())),
      buffer_(),
      input_stream_closed_(false),
      output_stream_closed_(false),
      read_all_chunks_(false) {}

Pipe::~Pipe() {
  // Deallocate all the chunks still left in buffer_.
  for (BufferType::iterator chunk_iter = buffer_.begin();
       chunk_iter != buffer_.end(); ++chunk_iter) {
    (*chunk_iter).destroy();
  }
}

Ptr<InputStream> Pipe::createInputStream(Ptr<Pipe> self) {
  assert(self.isRefCounted());
  return MakeRefCountedPtr(new pipe::PipeInputStream(self));
}

Ptr<OutputStream> Pipe::createOutputStream(Ptr<Pipe> self) {
  assert(self.isRefCounted());
  return MakeRefCountedPtr(new pipe::PipeOutputStream(self));
}

ExceptionOr<ConstPtr<ByteArray>> Pipe::read(std::int64_t size) {
  Synchronized s(lock_.get());

  // We're done reading all the chunks that were written before the OutputStream
  // was closed, so there's nothing to do here other than return an empty chunk
  // to serve as an EOF indication to callers.
  if (read_all_chunks_) {
    ExceptionOr<ConstPtr<ByteArray>>(ConstPtr<ByteArray>());
  }

  while (buffer_.empty() && !input_stream_closed_) {
    Exception::Value wait_exception = cond_->wait();

    if (Exception::NONE != wait_exception) {
      if (Exception::INTERRUPTED == wait_exception) {
        return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
      }
    }
  }

  if (input_stream_closed_) {
    return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
  }

  ScopedPtr<ConstPtr<ByteArray>> first_chunk(buffer_.front());
  buffer_.pop_front();

  // If we received our sentinel chunk, mark the fact that there cannot
  // possibly be any more chunks to read here on in, and return an empty chunk
  // to serve as an EOF indication to callers.
  if (first_chunk.isNull()) {
    read_all_chunks_ = true;
    return ExceptionOr<ConstPtr<ByteArray>>(ConstPtr<ByteArray>());
  }

  // If first_chunk is small enough to not overshoot the requested 'size', just
  // return that.
  if (first_chunk->size() <= size) {
    return ExceptionOr<ConstPtr<ByteArray>>(first_chunk.release());
  } else {
    // Break first_chunk into 2 parts -- the first one of which (next_chunk)
    // will be 'size' bytes long, and will be returned, and the second one of
    // which (overflow_chunk) will be re-inserted into buffer_, at the head of
    // the queue, to be served up in the next call to read().
    ScopedPtr<ConstPtr<ByteArray>> next_chunk(
        MakeConstPtr(new ByteArray(first_chunk->getData(), size)));
    ScopedPtr<ConstPtr<ByteArray>> overflow_chunk(MakeConstPtr(new ByteArray(
        first_chunk->getData() + size, first_chunk->size() - size)));
    buffer_.push_front(overflow_chunk.release());
    return ExceptionOr<ConstPtr<ByteArray>>(next_chunk.release());
  }
}

Exception::Value Pipe::write(ConstPtr<ByteArray> data) {
  Synchronized s(lock_.get());

  return writeLocked(data);
}

void Pipe::markInputStreamClosed() {
  Synchronized s(lock_.get());

  input_stream_closed_ = true;
  // Trigger cond_ to unblock a potentially-blocked call to read(), and to let
  // it know to return Exception::IO.
  cond_->notify();
}

void Pipe::markOutputStreamClosed() {
  Synchronized s(lock_.get());

  // Write a sentinel null chunk before marking output_stream_closed as true.
  writeLocked(ConstPtr<ByteArray>());
  output_stream_closed_ = true;
}

Exception::Value Pipe::writeLocked(ConstPtr<ByteArray> data) {
  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_data(data);

  if (eitherStreamClosed()) {
    return Exception::IO;
  }

  buffer_.push_back(scoped_data.release());
  // Trigger cond_ to unblock a potentially-blocked call to read(), now that
  // there's more data for it to consume.
  cond_->notify();
  return Exception::NONE;
}

bool Pipe::eitherStreamClosed() const {
  return input_stream_closed_ || output_stream_closed_;
}

}  // namespace nearby
}  // namespace location
