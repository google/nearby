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

#ifndef PLATFORM_PIPE_H_
#define PLATFORM_PIPE_H_

#include <cstdint>
#include <deque>

#include "platform/api/condition_variable.h"
#include "platform/api/input_stream.h"
#include "platform/api/lock.h"
#include "platform/api/output_stream.h"
#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

namespace pipe {

class PipeInputStream;
class PipeOutputStream;

}  // namespace pipe

class Pipe {
 public:
  Pipe();
  ~Pipe();

  // The returned InputStream is auto-destroyed when no longer referenced.
  static Ptr<InputStream> createInputStream(Ptr<Pipe>);
  // The returned OutputStream is auto-destroyed when no longer referenced.
  static Ptr<OutputStream> createOutputStream(Ptr<Pipe>);

 private:
  //////////////////////////////////////////////////////////////////////////////
  // Everything in this first private: section is only used by PipeInputStream
  // and PipeOutputStream, thus forming the interface presented to those 2
  // classes.
  //////////////////////////////////////////////////////////////////////////////

  friend class pipe::PipeInputStream;
  friend class pipe::PipeOutputStream;

  ExceptionOr<ConstPtr<ByteArray> > read(std::int64_t size);
  Exception::Value write(ConstPtr<ByteArray> data);

  void markInputStreamClosed();
  void markOutputStreamClosed();

 private:
  Exception::Value writeLocked(ConstPtr<ByteArray> data);

  bool eitherStreamClosed() const;

  ScopedPtr<Ptr<Lock> > lock_;
  ScopedPtr<Ptr<ConditionVariable> > cond_;
  typedef std::deque<ConstPtr<ByteArray> > BufferType;
  BufferType buffer_;
  bool input_stream_closed_;
  bool output_stream_closed_;
  bool read_all_chunks_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PIPE_H_
