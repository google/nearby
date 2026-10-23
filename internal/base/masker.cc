// Copyright 2025 Google LLC
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

#include "internal/base/masker.h"

#include <string>

#include "absl/strings/string_view.h"

namespace nearby {
namespace masker {
namespace {

// The default mask character to use for masking. This is a single asterisk.
constexpr char kDefaultMaskChar = '*';

// The default start index to use for masking.
constexpr int kDefaultMaskStartIndex = 2;

}  // namespace

std::string Mask(absl::string_view input) {
  return Mask(input, kDefaultMaskChar, kDefaultMaskStartIndex);
}

std::string Mask(absl::string_view input, char mask, int start_index) {
  if (start_index < 0) {
    start_index = 0;
  }
  if (start_index >= input.length()) {
    return std::string(input);
  }

  std::string result = std::string(input);
  for (int i = start_index; i < input.length(); ++i) {
    result[i] = mask;
  }

  return result;
}

}  // namespace masker
}  // namespace nearby
