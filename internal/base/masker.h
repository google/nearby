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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_MASKER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_MASKER_H_

#include <string>

#include "absl/strings/string_view.h"

namespace nearby {
namespace masker {

// Masks the input string with the default mask character '*' and start index 2.
std::string Mask(absl::string_view input);

// Masks the input string with the given mask character and start index.
std::string Mask(absl::string_view input, char mask, int start_index);

}  // namespace masker
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_MASKER_H_
