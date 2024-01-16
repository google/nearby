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

#include "sharing/internal/base/utf_string_conversions.h"

#include <limits.h>
#include <stdint.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

#include "third_party/icu_utf/icu_utf.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace utils {
namespace {

using MachineWord = uintptr_t;

constexpr int32_t kErrorCodePoint = 0xFFFD;

inline bool IsMachineWordAligned(const void* pointer) {
  return !(reinterpret_cast<MachineWord>(pointer) & (sizeof(MachineWord) - 1));
}

template <class Char>
bool DoIsStringAscii(const Char* characters, size_t length) {
  // Bitmasks to detect non-ASCII characters for character sizes of 8, 16 and 32
  // bits.
  constexpr MachineWord NonASCIIMasks[] = {
      0, MachineWord(0x8080808080808080ULL), MachineWord(0xFF80FF80FF80FF80ULL),
      0, MachineWord(0xFFFFFF80FFFFFF80ULL),
  };

  if (!length) return true;
  constexpr MachineWord non_ascii_bit_mask = NonASCIIMasks[sizeof(Char)];
  static_assert(non_ascii_bit_mask, "Error: Invalid Mask");
  MachineWord all_char_bits = 0;
  const Char* end = characters + length;

  // Prologue: align the input.
  while (!IsMachineWordAligned(characters) && characters < end)
    all_char_bits |= *characters++;
  if (all_char_bits & non_ascii_bit_mask) return false;

  // Compare the values of CPU word size.
  constexpr size_t chars_per_word = sizeof(MachineWord) / sizeof(Char);
  constexpr int batch_count = 16;
  while (characters <= end - batch_count * chars_per_word) {
    all_char_bits = 0;
    for (int i = 0; i < batch_count; ++i) {
      all_char_bits |= *(reinterpret_cast<const MachineWord*>(characters));
      characters += chars_per_word;
    }
    if (all_char_bits & non_ascii_bit_mask) return false;
  }

  // Process the remaining words.
  all_char_bits = 0;
  while (characters <= end - chars_per_word) {
    all_char_bits |= *(reinterpret_cast<const MachineWord*>(characters));
    characters += chars_per_word;
  }

  // Process the remaining bytes.
  while (characters < end) all_char_bits |= *characters++;

  return !(all_char_bits & non_ascii_bit_mask);
}

inline bool IsValidCharacter(uint32_t code_point) {
  // Excludes non-characters (U+FDD0..U+FDEF, and all code points
  // ending in 0xFFFE or 0xFFFF) from the set of valid code points.
  // https://unicode.org/faq/private_use.html#nonchar1
  return code_point < 0xD800u ||
         (code_point >= 0xE000u && code_point < 0xFDD0u) ||
         (code_point > 0xFDEFu && code_point <= 0x10FFFFu &&
          (code_point & 0xFFFEu) != 0xFFFEu);
}

template <bool (*Validator)(uint32_t)>
inline bool DoIsStringUtf8(std::string_view str) {
  const char* src = str.data();
  int32_t src_len = static_cast<int32_t>(str.length());
  int32_t char_index = 0;

  while (char_index < src_len) {
    int32_t code_point;
    CBU8_NEXT(src, char_index, src_len, code_point);
    if (!Validator(code_point)) return false;
  }
  return true;
}

// Size coefficient ----------------------------------------------------------
// The maximum number of codeunits in the destination encoding corresponding to
// one codeunit in the source encoding.

template <typename SrcChar, typename DestChar>
struct SizeCoefficient {
  static_assert(sizeof(SrcChar) < sizeof(DestChar),
                "Default case: from a smaller encoding to the bigger one");

  // ASCII symbols are encoded by one codeunit in all encodings.
  static constexpr int value = 1;
};

template <>
struct SizeCoefficient<char16_t, char> {
  // One UTF-16 code unit corresponds to at most 3 code units in UTF-8.
  static constexpr int value = 3;
};

#if defined(WCHAR_T_IS_UTF32)
template <>
struct SizeCoefficient<wchar_t, char> {
  // UTF-8 uses at most 4 code units per character.
  static constexpr int value = 4;
};

template <>
struct SizeCoefficient<wchar_t, char16_t> {
  // UTF-16 uses at most 2 code units per character.
  static constexpr int value = 2;
};
#endif  // defined(WCHAR_T_IS_UTF32)

template <typename SrcChar, typename DestChar>
constexpr int size_coefficient_v =
    SizeCoefficient<std::decay_t<SrcChar>, std::decay_t<DestChar>>::value;

// UnicodeAppendUnsafe --------------------------------------------------------
// Function overloads that write code_point to the output string. Output string
// has to have enough space for the codepoint.

// Convenience typedef that checks whether the passed in type is integral (i.e.
// bool, char, int or their extended versions) and is of the correct size.
template <typename Char, size_t N>
using EnableIfBitsAre = std::enable_if_t<
    std::is_integral<Char>::value && CHAR_BIT * sizeof(Char) == N, bool>;

template <typename Char, EnableIfBitsAre<Char, 8> = true>
void UnicodeAppendUnsafe(Char* out, int32_t* size, uint32_t code_point) {
  CBU8_APPEND_UNSAFE(out, *size, code_point);
}

template <typename Char, EnableIfBitsAre<Char, 16> = true>
void UnicodeAppendUnsafe(Char* out, int32_t* size, uint32_t code_point) {
  CBU16_APPEND_UNSAFE(out, *size, code_point);
}

template <typename Char, EnableIfBitsAre<Char, 32> = true>
void UnicodeAppendUnsafe(Char* out, int32_t* size, uint32_t code_point) {
  out[(*size)++] = code_point;
}

// DoUtfConversion ------------------------------------------------------------
// Main driver of UtfConversion specialized for different Src encodings.
// dest has to have enough room for the converted text.

template <typename DestChar>
bool DoUtfConversion(const char* src, int32_t src_len, DestChar* dest,
                     int32_t* dest_len) {
  bool success = true;

  for (int32_t i = 0; i < src_len;) {
    int32_t code_point;
    CBU8_NEXT(src, i, src_len, code_point);

    if (!IsValidCodepoint(code_point)) {
      success = false;
      code_point = kErrorCodePoint;
    }

    UnicodeAppendUnsafe(dest, dest_len, code_point);
  }

  return success;
}

template <typename DestChar>
bool DoUtfConversion(const char16_t* src, int32_t src_len, DestChar* dest,
                     int32_t* dest_len) {
  bool success = true;

  auto ConvertSingleChar = [&success](char16_t in) -> int32_t {
    if (!CBU16_IS_SINGLE(in) || !IsValidCodepoint(in)) {
      success = false;
      return kErrorCodePoint;
    }
    return in;
  };

  int32_t i = 0;

  // Always have another symbol in order to avoid checking boundaries in the
  // middle of the surrogate pair.
  while (i < src_len - 1) {
    int32_t code_point;

    if (CBU16_IS_LEAD(src[i]) && CBU16_IS_TRAIL(src[i + 1])) {
      code_point = CBU16_GET_SUPPLEMENTARY(src[i], src[i + 1]);
      if (!IsValidCodepoint(code_point)) {
        code_point = kErrorCodePoint;
        success = false;
      }
      i += 2;
    } else {
      code_point = ConvertSingleChar(src[i]);
      ++i;
    }

    UnicodeAppendUnsafe(dest, dest_len, code_point);
  }

  if (i < src_len)
    UnicodeAppendUnsafe(dest, dest_len, ConvertSingleChar(src[i]));

  return success;
}

#if defined(WCHAR_T_IS_UTF32)

template <typename DestChar>
bool DoUtfConversion(const wchar_t* src, int32_t src_len, DestChar* dest,
                     int32_t* dest_len) {
  bool success = true;

  for (int32_t i = 0; i < src_len; ++i) {
    int32_t code_point = src[i];

    if (!IsValidCodepoint(code_point)) {
      success = false;
      code_point = kErrorCodePoint;
    }

    UnicodeAppendUnsafe(dest, dest_len, code_point);
  }

  return success;
}

#endif  // defined(WCHAR_T_IS_UTF32)

// UtfConversion --------------------------------------------------------------
// Function template for generating all UTF conversions.

template <typename InputString, typename DestString>
bool UtfConversion(const InputString& src_str, DestString* dest_str) {
  if (IsStringAscii(src_str)) {
    dest_str->assign(src_str.begin(), src_str.end());
    return true;
  }

  dest_str->resize(src_str.length() *
                   size_coefficient_v<typename InputString::value_type,
                                      typename DestString::value_type>);

  // Empty string is ASCII => it OK to call operator[].
  auto* dest = &(*dest_str)[0];

  // ICU requires 32 bit numbers.
  int32_t src_len32 = static_cast<int32_t>(src_str.length());
  int32_t dest_len32 = 0;

  bool res = DoUtfConversion(src_str.data(), src_len32, dest, &dest_len32);

  dest_str->resize(dest_len32);
  dest_str->shrink_to_fit();

  return res;
}

#if defined(WCHAR_T_IS_UTF16)
inline const char16_t* as_u16cstr(const wchar_t* str) {
  return reinterpret_cast<const char16_t*>(str);
}

inline const char16_t* as_u16cstr(std::wstring_view str) {
  return reinterpret_cast<const char16_t*>(str.data());
}
#endif

}  // namespace

// UTF16 <-> UTF8 --------------------------------------------------------------

bool Utf8ToUtf16(const char* src, size_t src_len, std::u16string* output) {
  return UtfConversion(std::string_view(src, src_len), output);
}

std::u16string Utf8ToUtf16(std::string_view utf8) {
  std::u16string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  Utf8ToUtf16(utf8.data(), utf8.size(), &ret);
  return ret;
}

bool Utf16ToUtf8(const char16_t* src, size_t src_len, std::string* output) {
  return UtfConversion(std::u16string_view(src, src_len), output);
}

std::string Utf16ToUtf8(std::u16string_view utf16) {
  std::string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  Utf16ToUtf8(utf16.data(), utf16.length(), &ret);
  return ret;
}

// UTF-16 <-> Wide -------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF16)
// When wide == UTF-16 the conversions are a NOP.

bool WideToUtf16(const wchar_t* src, size_t src_len, std::u16string* output) {
  output->assign(src, src + src_len);
  return true;
}

std::u16string WideToUtf16(std::wstring_view wide) {
  return std::u16string(wide.begin(), wide.end());
}

bool Utf16ToWide(const char16_t* src, size_t src_len, std::wstring* output) {
  output->assign(src, src + src_len);
  return true;
}

std::wstring Utf16ToWide(std::u16string_view utf16) {
  return std::wstring(utf16.begin(), utf16.end());
}

#elif defined(WCHAR_T_IS_UTF32)

bool WideToUtf16(const wchar_t* src, size_t src_len, std::u16string* output) {
  return UtfConversion(std::wstring_view(src, src_len), output);
}

std::u16string WideToUtf16(std::wstring_view wide) {
  std::u16string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  WideToUtf16(wide.data(), wide.length(), &ret);
  return ret;
}

bool Utf16ToWide(const char16_t* src, size_t src_len, std::wstring* output) {
  return UtfConversion(std::u16string_view(src, src_len), output);
}

std::wstring Utf16ToWide(std::u16string_view utf16) {
  std::wstring ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  Utf16ToWide(utf16.data(), utf16.length(), &ret);
  return ret;
}

#endif  // defined(WCHAR_T_IS_UTF32)

// UTF-8 <-> Wide --------------------------------------------------------------

// UTF8ToWide is the same code, regardless of whether wide is 16 or 32 bits

bool Utf8ToWide(const char* src, size_t src_len, std::wstring* output) {
  return UtfConversion(std::string_view(src, src_len), output);
}

std::wstring Utf8ToWide(std::string_view utf8) {
  std::wstring ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  Utf8ToWide(utf8.data(), utf8.length(), &ret);
  return ret;
}

#if defined(WCHAR_T_IS_UTF16)
// Easy case since we can use the "utf" versions we already wrote above.

bool WideToUtf8(const wchar_t* src, size_t src_len, std::string* output) {
  return Utf16ToUtf8(as_u16cstr(src), src_len, output);
}

std::string WideToUtf8(std::wstring_view wide) {
  return Utf16ToUtf8(std::u16string_view(as_u16cstr(wide), wide.size()));
}

#elif defined(WCHAR_T_IS_UTF32)

bool WideToUtf8(const wchar_t* src, size_t src_len, std::string* output) {
  return UtfConversion(std::wstring_view(src, src_len), output);
}

std::string WideToUtf8(std::wstring_view wide) {
  std::string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  WideToUtf8(wide.data(), wide.length(), &ret);
  return ret;
}

#endif  // defined(WCHAR_T_IS_UTF32)

std::u16string AsciiToUtf16(std::string_view ascii) {
  NL_DCHECK(IsStringAscii(ascii));
  return std::u16string(ascii.begin(), ascii.end());
}

std::string Utf16ToAscii(std::u16string_view utf16) {
  NL_DCHECK(IsStringAscii(utf16));
  return std::string(utf16.begin(), utf16.end());
}

#if defined(WCHAR_T_IS_UTF16)
std::wstring AsciiToWide(std::string_view ascii) {
  NL_DCHECK(IsStringAscii(ascii));
  return std::wstring(ascii.begin(), ascii.end());
}

std::string WideToAscii(std::string_view wide) {
  NL_DCHECK(IsStringAscii(wide));
  return std::string(wide.begin(), wide.end());
}
#endif  // defined(WCHAR_T_IS_UTF16)

bool IsStringAscii(std::string_view str) {
  return DoIsStringAscii(str.data(), str.length());
}

bool IsStringAscii(std::u16string_view str) {
  return DoIsStringAscii(str.data(), str.length());
}

bool IsStringUtf8(std::string_view str) {
  return DoIsStringUtf8<IsValidCharacter>(str);
}

bool IsStringAscii(std::wstring_view str) {
  return DoIsStringAscii(str.data(), str.length());
}

bool IsValidCodepoint(uint32_t code_point) {
  // Excludes code points that are not Unicode scalar values, i.e.
  // surrogate code points ([0xD800, 0xDFFF]). Additionally, excludes
  // code points larger than 0x10FFFF (the highest codepoint allowed).
  // Non-characters and unassigned code points are allowed.
  // https://unicode.org/glossary/#unicode_scalar_value
  return code_point < 0xD800u ||
         (code_point >= 0xE000u && code_point <= 0x10FFFFu);
}

void TruncateUtf8ToByteSize(const std::string& input, size_t byte_size,
                            std::string* output) {
  NL_DCHECK(output);
  if (byte_size > input.length()) {
    *output = input;
    return;
  }

  // Note: This cast is necessary because CBU8_NEXT uses int32_ts.
  int32_t truncation_length = static_cast<int32_t>(byte_size);
  int32_t char_index = truncation_length - 1;
  const char* data = input.data();

  // Using CBU8, we will move backwards from the truncation point
  // to the beginning of the string looking for a valid UTF8
  // character.  Once a full UTF8 character is found, we will
  // truncate the string to the end of that character.
  while (char_index >= 0) {
    int32_t prev = char_index;
    int32_t code_point = 0;
    CBU8_NEXT(data, char_index, truncation_length, code_point);
    if (!IsValidCharacter(code_point) || !IsValidCodepoint(code_point)) {
      char_index = prev - 1;
    } else {
      break;
    }
  }

  if (char_index >= 0)
    *output = input.substr(0, char_index);
  else
    output->clear();
}

std::string ToString(const char* str) {
  if (str == nullptr) {
    return "";
  }
  return std::string(str);
}

}  // namespace utils
}  // namespace nearby
