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

#ifndef PLATFORM_BASE_BASE_PIPE_H_
#define PLATFORM_BASE_BASE_PIPE_H_

#include <cstdint>
#include <deque>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {

// Common Pipe implementation.
// It does not depend on platform implementation, and this allows it to
// be used in the platform implementation itself.
// Concrete class must be derived from it, as follows:
//
// class DerivedPipe : public BasePipe {
//  public:
//   DerivedPipe() {
//     auto mutex = /* construct platform-dependent mutex */;
//     auto cond = /* construct platform-dependent condition variable */;
//     Setup(std::move(mutex), std::move(cond));
//   }
//   ~DerivedPipe() override = default;
//   DerivedPipe(DerivedPipe&&) = default;
//   DerivedPipe& operator=(DerivedPipe&&) = default;
// };
class BasePipe {
 public:
  static constexpr const size_t kChunkSize = 64 * 1024;
  virtual ~BasePipe() = default;

  // Pipe is not copyable or movable, because copy/move will invalidate
  // references to input and output streams.
  // If move is required, Pipe could be wrapped with std::unique_ptr<>.
  BasePipe(BasePipe&&) = delete;
  BasePipe& operator=(BasePipe&&) = delete;

  // Get...() methods return references to input and output steam facades.
  // It is safe to call Get...() methods multiple times.
  InputStream& GetInputStream() { return input_stream_; }
  OutputStream& GetOutputStream() { return output_stream_; }

 protected:
  BasePipe() = default;

  void Setup(std::unique_ptr<api::Mutex> mutex,
             std::unique_ptr<api::ConditionVariable> cond) {
    mutex_ = std::move(mutex);
    cond_ = std::move(cond);
  }

 private:
  class BasePipeInputStream : public InputStream {
   public:
    explicit BasePipeInputStream(BasePipe* pipe) : pipe_(pipe) {}
    ~BasePipeInputStream() override { DoClose(); }

    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return pipe_->Read(size);
    }
    Exception Close() override { return DoClose(); }

   private:
    Exception DoClose() {
      pipe_->MarkInputStreamClosed();
      return {Exception::kSuccess};
    }
    BasePipe* pipe_;
  };
  class BasePipeOutputStream : public OutputStream {
   public:
    explicit BasePipeOutputStream(BasePipe* pipe) : pipe_(pipe) {}
    ~BasePipeOutputStream() override { DoClose(); }

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
    BasePipe* pipe_;
  };

  ExceptionOr<ByteArray> Read(size_t size) ABSL_LOCKS_EXCLUDED(mutex_);
  Exception Write(const ByteArray& data) ABSL_LOCKS_EXCLUDED(mutex_);

  void MarkInputStreamClosed() ABSL_LOCKS_EXCLUDED(mutex_);
  void MarkOutputStreamClosed() ABSL_LOCKS_EXCLUDED(mutex_);

  Exception WriteLocked(const ByteArray& data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Order of declaration matters:
  // - mutex must be defined before condvar;
  // - input & output streams must be after both mutex and condvar.
  bool input_stream_closed_ ABSL_GUARDED_BY(mutex_) = false;
  bool output_stream_closed_ ABSL_GUARDED_BY(mutex_) = false;
  bool read_all_chunks_ ABSL_GUARDED_BY(mutex_) = false;

  std::deque<ByteArray> ABSL_GUARDED_BY(mutex_) buffer_;
  std::unique_ptr<api::Mutex> mutex_;
  std::unique_ptr<api::ConditionVariable> cond_;

  BasePipeInputStream input_stream_{this};
  BasePipeOutputStream output_stream_{this};
};

}  // namespace nearby

#endif  // PLATFORM_BASE_BASE_PIPE_H_
