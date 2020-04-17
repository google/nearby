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

#ifndef PLATFORM_API2_INPUT_STREAM_H_
#define PLATFORM_API2_INPUT_STREAM_H_

#include <cstdint>

#include "platform/byte_array.h"
#include "platform/exception.h"

namespace location {
namespace nearby {

// An InputStream represents an input stream of bytes.
//
// https://docs.oracle.com/javase/8/docs/api/java/io/InputStream.html
class InputStream {
 public:
  virtual ~InputStream() {}

  virtual ExceptionOr<ByteArray> Read(
      size_t size) = 0;           // throws Exception::kIo
  virtual Exception Close() = 0;  // throws Exception::kIo
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_INPUT_STREAM_H_
