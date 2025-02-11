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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_ENCODING_BASE85_H_
#define THIRD_PARTY_NEARBY_INTERNAL_ENCODING_BASE85_H_

#include <optional>
#include <string>

namespace nearby {
namespace encoding {

// Encodes the input string to Base85. If pad is true, the output string will
// not be truncated.
std::string Base85Encode(const std::string& input, bool padding = false);

// Decodes the input string from Base85.
std::optional<std::string> Base85Decode(const std::string& input);

}  // namespace encoding
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_ENCODING_BASE85_H_
