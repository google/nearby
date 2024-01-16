// Copyright 2021 Google LLC
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

#include <stddef.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "sharing/internal/base/utf_string_configuration.h"

namespace nearby {
namespace utils {

// These convert between UTF-8, -16, and -32 strings. They are potentially slow,
// so avoid unnecessary conversions. The low-level versions return a boolean
// indicating whether the conversion was 100% valid. In this case, it will still
// do the best it can and put the result in the output buffer. The versions that
// return strings ignore this error and just return the best conversion
// possible.
bool WideToUtf8(const wchar_t* src, size_t src_len, std::string* output);
std::string WideToUtf8(std::wstring_view wide);
bool Utf8ToWide(const char* src, size_t src_len, std::wstring* output);
std::wstring Utf8ToWide(std::string_view utf8);

bool WideToUtf16(const wchar_t* src, size_t src_len, std::u16string* output);
std::u16string WideToUtf16(std::wstring_view wide);
bool Utf16ToWide(const char16_t* src, size_t src_len, std::wstring* output);
std::wstring Utf16ToWide(std::u16string_view utf16);

bool Utf8ToUtf16(const char* src, size_t src_len, std::u16string* output);
std::u16string Utf8ToUtf16(std::string_view utf8);
bool Utf16ToUtf8(const char16_t* src, size_t src_len, std::string* output);
std::string Utf16ToUtf8(std::u16string_view utf16);

// This converts an ASCII string, typically a hard coded constant, to a UTF16
// string.
std::u16string AsciiToUtf16(std::string_view ascii);

// Converts to 7-bit ASCII by truncating. The result must be known to be ASCII
// beforehand.
std::string Utf16ToAscii(std::u16string_view utf16);

bool IsStringAscii(std::string_view str);
bool IsStringAscii(std::u16string_view str);
bool IsStringAscii(std::wstring_view str);
bool IsStringUtf8(std::string_view str);

inline bool IsValidCodepoint(uint32_t code_point);

void TruncateUtf8ToByteSize(const std::string& input, size_t byte_size,
                            std::string* output);

#if defined(WCHAR_T_IS_UTF16)
// This converts an ASCII string, typically a hard coded constant, to a wide
// string.
std::wstring AsciiToWide(std::string_view ascii);

// Converts to 7-bit ASCII by truncating. The result must be known to be ASCII
// beforehand.
std::string WideToAscii(std::wstring_view wide);
#endif  // defined(WCHAR_T_IS_UTF16)

// The conversion functions in this file should not be used to convert string
// literals. Instead, the corresponding prefixes (e.g. u"" for UTF16 or L"" for
// Wide) should be used. Deleting the overloads here catches these cases at
// compile time.
template <size_t N>
std::u16string WideToUtf16(const wchar_t (&str)[N]) {
  static_assert(N == 0, "Error: Use the u\"...\" prefix instead.");
  return std::u16string();
}

template <size_t N>
std::u16string Utf8ToUtf16(const char (&str)[N]) {
  static_assert(N == 0, "Error: Use the u\"...\" prefix instead.");
  return std::u16string();
}

template <size_t N>
std::u16string AsciiToUtf16(const char (&str)[N]) {
  static_assert(N == 0, "Error: Use the u\"...\" prefix instead.");
  return std::u16string();
}

// Mutable character arrays are usually only populated during runtime. Continue
// to allow this conversion.
template <size_t N>
std::u16string AsciiToUtf16(char (&str)[N]) {
  return AsciiToUtf16(std::string_view(str));
}

template <typename T, size_t N>
constexpr size_t size(const T (&array)[N]) noexcept {
  return N;
}

std::string ToString(const char* str);

}  // namespace utils
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_UTF_STRING_CONVERSIONS_H_
