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

#ifndef PLATFORM_API_OUTPUT_STREAM_H_
#define PLATFORM_API_OUTPUT_STREAM_H_

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// An OutputStream represents an output stream of bytes.
//
// https://docs.oracle.com/javase/8/docs/api/java/io/OutputStream.html
class OutputStream {
 public:
  virtual ~OutputStream() {}

  // Takes ownership of the passed-in ConstPtr, and ensures that it is destroyed
  // even upon error (i.e. when the return value is not Exception::NONE).
  virtual Exception::Value write(
      ConstPtr<ByteArray> data) = 0;     // throws Exception::IO
  virtual Exception::Value flush() = 0;  // throws Exception::IO
  virtual Exception::Value close() = 0;  // throws Exception::IO
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_OUTPUT_STREAM_H_
