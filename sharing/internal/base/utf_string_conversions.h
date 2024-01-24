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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_UTF_STRING_CONVERSIONS_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_UTF_STRING_CONVERSIONS_H_

#if defined(GITHUB_BUILD)
// Stub out string conversion functions for github builds.
namespace nearby::utils {
std::string WideToUtf8(std::wstring_view wide) { return std::string(); }
std::wstring Utf8ToWide(std::string_view utf8) { return std::wstring(); }

bool IsStringUtf8(std::string_view str) { return true; }

void TruncateUtf8ToByteSize(const std::string& input, size_t byte_size,
                            std::string* output) {}

}  // namespace nearby::utils

#elif defined(NEARBY_CHROMIUM)
// Forward to chromium implementations.

#else  // defined(GITHUB_BUILD)
#include "sharing/internal/base/strings/utf_string_conversions.h"  // IWYU pragma: export
#endif  // defined(GITHUB_BUILD)

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_UTF_STRING_CONVERSIONS_H_
