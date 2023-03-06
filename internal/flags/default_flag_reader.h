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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_FLAGS_DEFAULT_FLAG_READER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_FLAGS_DEFAULT_FLAG_READER_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/flags/flag_reader.h"

namespace nearby {
namespace flags {

class DefaultFlagReader : public FlagReader {
 public:
  ~DefaultFlagReader() override = default;

  // Returns the default bool value of the flag.
  bool GetBoolFlag(const Flag<bool>& flag) override {
    return flag.default_value();
  }

  // Returns the default int64_t value of the flag.
  int64_t GetInt64Flag(const Flag<int64_t>& flag) override {
    return flag.default_value();
  }

  // Returns the default double value of the flag.
  double GetDoubleFlag(const Flag<double>& flag) override {
    return flag.default_value();
  }

  // Returns the default string value of the flag.
  std::string GetStringFlag(const Flag<absl::string_view>& flag) override {
    return std::string(flag.default_value());
  }
};

}  // namespace flags
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_FLAGS_DEFAULT_FLAG_READER_H_
