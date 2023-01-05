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

#include <cstdint>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"

namespace nearby {
namespace api {

// An InputFile represents a readable file on the system.
class InputFile : public InputStream {
 public:
  ~InputFile() override = default;
  virtual std::string GetFilePath() const = 0;
  virtual std::int64_t GetTotalSize() const = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_INPUT_FILE_H_
