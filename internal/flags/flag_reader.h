// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_FLAGS_FLAG_READER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_FLAGS_FLAG_READER_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/flags/flag.h"

namespace nearby {
namespace flags {

// Allows caller to get flag values form backend system.
// The caller could implement flexible control on Nearby SDK in granularity,
// such as feature, configuration etc. Default value will be returned if no
// flag-supported backend.
class FlagReader {
 public:
  virtual ~FlagReader() = default;

  // Reads flag with boolean value.
  virtual bool GetBoolFlag(const Flag<bool>& flag) = 0;

  // Reads flag with int64_t value.
  virtual int64_t GetInt64Flag(const Flag<int64_t>& flag) = 0;

  // Reads flag with double value.
  virtual double GetDoubleFlag(const Flag<double>& flag) = 0;

  // Reads flag with string value.
  virtual std::string GetStringFlag(const Flag<absl::string_view>& flag) = 0;
};

}  // namespace flags
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_FLAGS_FLAG_READER_H_
