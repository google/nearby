// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_COMMON_COMPATIBLE_U8_STRING_H_
#define THIRD_PARTY_NEARBY_SHARING_COMMON_COMPATIBLE_U8_STRING_H_

#include <string>

namespace nearby {
namespace sharing {

#if defined(__cpp_lib_char8_t)
inline std::string GetCompatibleU8String(std::u8string str) {
  return reinterpret_cast<const char*>(str.c_str());
}
#else
inline std::string GetCompatibleU8String(std::string str) { return str; }
#endif

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_COMMON_COMPATIBLE_U8_STRING_H_
