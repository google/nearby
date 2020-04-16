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

#ifndef PLATFORM_API_INPUT_FILE_H_
#define PLATFORM_API_INPUT_FILE_H_

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// An InputFile represents a readable file on the system.
class InputFile {
 public:
  virtual ~InputFile() {}

  // The returned ConstPtr will be owned (and destroyed) by the caller.
  // When we have exhausted reading the file and no bytes remain, read will
  // always return an empty ConstPtr for which isNull() is true.
  virtual ExceptionOr<ConstPtr<ByteArray>> read(
      std::int64_t size) = 0;  // throws Exception::IO when the file cannot be
                               // opened or read.
  virtual std::string getFilePath() const = 0;
  virtual std::int64_t getTotalSize() const = 0;
  virtual void close() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_INPUT_FILE_H_
