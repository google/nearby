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

#include "platform/synchronized.h"

namespace location {
namespace nearby {

namespace pipe {

template <typename Platform>
class PipeInputStream : public InputStream {
 public:
  explicit PipeInputStream(Ptr<Pipe<Platform>> pipe) : pipe_(pipe) {}
  ~PipeInputStream() override {
    close();
  }
  ExceptionOr<ConstPtr<ByteArray>> read() override { return read(kChunkSize); }

  ExceptionOr<ConstPtr<ByteArray>> read(std::int64_t size) override {
    return pipe_->read(size);
  }

  Exception::Value close() override {
    pipe_->markInputStreamClosed();

    return Exception::NONE;
  }

 private:
  static const std::int64_t kChunkSize = 64 * 1024;

  Ptr<Pipe<Platform>> pipe_;
};

template <typename Platform>
class PipeOutputStream : public OutputStream {
 public:
  explicit PipeOutputStream(Ptr<Pipe<Platform>> pipe) : pipe_(pipe) {}
  ~PipeOutputStream() override {
    close();
  }

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
  Ptr<Pipe<Platform>> pipe_;
};

}  // namespace pipe

template <typename Platform>
Pipe<Platform>::Pipe()
    : lock_(Platform::createLock()),
      cond_(Platform::createConditionVariable(lock_.get())),
      buffer_(),
      input_stream_closed_(false),
      output_stream_closed_(false),
      read_all_chunks_(false) {}

template <typename Platform>
Pipe<Platform>::~Pipe() {
  // Deallocate all the chunks still left in buffer_.
  for (BufferType::iterator chunk_iter = buffer_.begin();
       chunk_iter != buffer_.end(); ++chunk_iter) {
    (*chunk_iter).destroy();
  }
}

template <typename Platform>
Ptr<InputStream> Pipe<Platform>::createInputStream(Ptr<Pipe> self) {
  assert(self.isRefCounted());
  return MakeRefCountedPtr(new pipe::PipeInputStream<Platform>(self));
}

template <typename Platform>
Ptr<OutputStream> Pipe<Platform>::createOutputStream(Ptr<Pipe> self) {
  assert(self.isRefCounted());
  return MakeRefCountedPtr(new pipe::PipeOutputStream<Platform>(self));
}

template <typename Platform>
ExceptionOr<ConstPtr<ByteArray>> Pipe<Platform>::read(std::int64_t size) {
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

template <typename Platform>
Exception::Value Pipe<Platform>::write(ConstPtr<ByteArray> data) {
  Synchronized s(lock_.get());

  return writeLocked(data);
}

template <typename Platform>
void Pipe<Platform>::markInputStreamClosed() {
  Synchronized s(lock_.get());

  input_stream_closed_ = true;
  // Trigger cond_ to unblock a potentially-blocked call to read(), and to let
  // it know to return Exception::IO.
  cond_->notify();
}

template <typename Platform>
void Pipe<Platform>::markOutputStreamClosed() {
  Synchronized s(lock_.get());

  // Write a sentinel null chunk before marking output_stream_closed as true.
  writeLocked(ConstPtr<ByteArray>());
  output_stream_closed_ = true;
}

template <typename Platform>
Exception::Value Pipe<Platform>::writeLocked(ConstPtr<ByteArray> data) {
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

template <typename Platform>
bool Pipe<Platform>::eitherStreamClosed() const {
  return input_stream_closed_ || output_stream_closed_;
}

}  // namespace nearby
}  // namespace location
