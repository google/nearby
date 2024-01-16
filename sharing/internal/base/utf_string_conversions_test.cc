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

#include <cstring>
#include <cwchar>
#include <sstream>
#include <string>

#include "gtest/gtest.h"

namespace nearby {
namespace utils {
namespace {

const wchar_t* const kConvertRoundtripCases[] = {
    L"Google Video",
    // "网页 图片 资讯更多 »"
    L"\x7f51\x9875\x0020\x56fe\x7247\x0020\x8d44\x8baf\x66f4\x591a\x0020\x00bb",
    //  "Παγκόσμιος Ιστός"
    L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
    L"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2",
    // "Поиск страниц на русском"
    L"\x041f\x043e\x0438\x0441\x043a\x0020\x0441\x0442"
    L"\x0440\x0430\x043d\x0438\x0446\x0020\x043d\x0430"
    L"\x0020\x0440\x0443\x0441\x0441\x043a\x043e\x043c",
    // "전체서비스"
    L"\xc804\xccb4\xc11c\xbe44\xc2a4",

// Test characters that take more than 16 bits. This will depend on whether
// wchar_t is 16 or 32 bits.
#if defined(WCHAR_T_IS_UTF16)
    L"\xd800\xdf00",
    // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 :
    // A,B,C,D,E)
    L"\xd807\xdd40\xd807\xdd41\xd807\xdd42\xd807\xdd43\xd807\xdd44",
#elif defined(WCHAR_T_IS_UTF32)
    L"\x10300",
    // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 :
    // A,B,C,D,E)
    L"\x11d40\x11d41\x11d42\x11d43\x11d44",
#endif
};

// Helper used to test TruncateUtf8ToByteSize.
bool Truncated(const std::string& input, const size_t byte_size,
               std::string* output) {
  size_t prev = input.length();
  TruncateUtf8ToByteSize(input, byte_size, output);
  return prev != output->length();
}

}  // namespace

TEST(UtfStringConversionsTest, ConvertUtf8AndWide) {
  // we round-trip all the wide strings through UTF-8 to make sure everything
  // agrees on the conversion. This uses the stream operators to test them
  // simultaneously.
  for (auto* i : kConvertRoundtripCases) {
    std::ostringstream utf8;
    utf8 << WideToUtf8(i);
    std::wostringstream wide;
    wide << Utf8ToWide(utf8.str());

    EXPECT_EQ(i, wide.str());
  }
}

TEST(UtfStringConversionsTest, ConvertUtf8AndWideEmptyString) {
  // An empty std::wstring should be converted to an empty std::string,
  // and vice versa.
  std::wstring wempty;
  std::string empty;
  EXPECT_EQ(empty, WideToUtf8(wempty));
  EXPECT_EQ(wempty, Utf8ToWide(empty));
}

TEST(UtfStringConversionsTest, ConvertUtf8ToWide) {
  struct Utf8ToWideCase {
    const char* utf8;
    const wchar_t* wide;
    bool success;
  } convert_cases[] = {
    // Regular UTF-8 input.
    {"\xe4\xbd\xa0\xe5\xa5\xbd", L"\x4f60\x597d", true},
    // Non-character is passed through.
    {"\xef\xbf\xbfHello", L"\xffffHello", true},
    // Truncated UTF-8 sequence.
    {"\xe4\xa0\xe5\xa5\xbd", L"\xfffd\x597d", false},
    // Truncated off the end.
    {"\xe5\xa5\xbd\xe4\xa0", L"\x597d\xfffd", false},
    // Non-shortest-form UTF-8.
    {"\xf0\x84\xbd\xa0\xe5\xa5\xbd", L"\xfffd\xfffd\xfffd\xfffd\x597d", false},
    // This UTF-8 character is decoded to a UTF-16 surrogate, which is illegal.
    {"\xed\xb0\x80", L"\xfffd\xfffd\xfffd", false},
  // Non-BMP characters. The second is a non-character regarded as valid.
  // The result will either be in UTF-16 or UTF-32.
#if defined(WCHAR_T_IS_UTF16)
    {"A\xF0\x90\x8C\x80z", L"A\xd800\xdf00z", true},
    {"A\xF4\x8F\xBF\xBEz", L"A\xdbff\xdffez", true},
#elif defined(WCHAR_T_IS_UTF32)
    {"A\xF0\x90\x8C\x80z", L"A\x10300z", true},
    {"A\xF4\x8F\xBF\xBEz", L"A\x10fffez", true},
#endif
  };

  for (const auto& i : convert_cases) {
    std::wstring converted;
    EXPECT_EQ(i.success, Utf8ToWide(i.utf8, strlen(i.utf8), &converted));
    std::wstring expected(i.wide);
    EXPECT_EQ(expected, converted);
  }

  // Manually test an embedded NULL.
  std::wstring converted;
  EXPECT_TRUE(Utf8ToWide("\00Z\t", 3, &converted));
  ASSERT_EQ(3U, converted.length());
  EXPECT_EQ(static_cast<wchar_t>(0), converted[0]);
  EXPECT_EQ('Z', converted[1]);
  EXPECT_EQ('\t', converted[2]);

  // Make sure that conversion replaces, not appends.
  EXPECT_TRUE(Utf8ToWide("B", 1, &converted));
  ASSERT_EQ(1U, converted.length());
  EXPECT_EQ('B', converted[0]);
}

#if defined(WCHAR_T_IS_UTF16)
// This test is only valid when wchar_t == UTF-16.
TEST(UtfStringConversionsTest, ConvertUtf16ToUtf8) {
  struct WideToUtf8Case {
    const wchar_t* utf16;
    const char* utf8;
    bool success;
  } convert_cases[] = {
      // Regular UTF-16 input.
      {L"\x4f60\x597d", "\xe4\xbd\xa0\xe5\xa5\xbd", true},
      // Test a non-BMP character.
      {L"\xd800\xdf00", "\xF0\x90\x8C\x80", true},
      // Non-characters are passed through.
      {L"\xffffHello", "\xEF\xBF\xBFHello", true},
      {L"\xdbff\xdffeHello", "\xF4\x8F\xBF\xBEHello", true},
      // The first character is a truncated UTF-16 character.
      {L"\xd800\x597d", "\xef\xbf\xbd\xe5\xa5\xbd", false},
      // Truncated at the end.
      {L"\x597d\xd800", "\xe5\xa5\xbd\xef\xbf\xbd", false},
  };

  for (const auto& test : convert_cases) {
    std::string converted;
    EXPECT_EQ(test.success,
              WideToUtf8(test.utf16, wcslen(test.utf16), &converted));
    std::string expected(test.utf8);
    EXPECT_EQ(expected, converted);
  }
}

#elif defined(WCHAR_T_IS_UTF32)
// This test is only valid when wchar_t == UTF-32.
TEST(UtfStringConversionsTest, ConvertUtf32ToUtf8) {
  struct WideToUtf8Case {
    const wchar_t* utf32;
    const char* utf8;
    bool success;
  } convert_cases[] = {
      // Regular 16-bit input.
      {L"\x4f60\x597d", "\xe4\xbd\xa0\xe5\xa5\xbd", true},
      // Test a non-BMP character.
      {L"A\x10300z", "A\xF0\x90\x8C\x80z", true},
      // Non-characters are passed through.
      {L"\xffffHello", "\xEF\xBF\xBFHello", true},
      {L"\x10fffeHello", "\xF4\x8F\xBF\xBEHello", true},
      // Invalid Unicode code points.
      {L"\xfffffffHello", "\xEF\xBF\xBDHello", false},
      // The first character is a truncated UTF-16 character.
      {L"\xd800\x597d", "\xef\xbf\xbd\xe5\xa5\xbd", false},
      {L"\xdc01Hello", "\xef\xbf\xbdHello", false},
  };

  for (const auto& test : convert_cases) {
    std::string converted;
    EXPECT_EQ(test.success,
              WideToUtf8(test.utf32, wcslen(test.utf32), &converted));
    std::string expected(test.utf8);
    EXPECT_EQ(expected, converted);
  }
}
#endif  // defined(WCHAR_T_IS_UTF32)

TEST(UtfStringConversionsTest, TruncateUtf8ToByteSize) {
  std::string output;

  // Empty strings and invalid byte_size arguments
  EXPECT_FALSE(Truncated(std::string(), 0, &output));
  EXPECT_EQ(output, "");
  EXPECT_TRUE(Truncated("\xe1\x80\xbf", 0, &output));
  EXPECT_EQ(output, "");
  EXPECT_FALSE(Truncated("\xe1\x80\xbf", static_cast<size_t>(-1), &output));
  EXPECT_FALSE(Truncated("\xe1\x80\xbf", 4, &output));

  // Testing the truncation of valid UTF8 correctly
  EXPECT_TRUE(Truncated("abc", 2, &output));
  EXPECT_EQ(output, "ab");
  EXPECT_TRUE(Truncated("\xc2\x81\xc2\x81", 2, &output));
  EXPECT_EQ(output.compare("\xc2\x81"), 0);
  EXPECT_TRUE(Truncated("\xc2\x81\xc2\x81", 3, &output));
  EXPECT_EQ(output.compare("\xc2\x81"), 0);
  EXPECT_FALSE(Truncated("\xc2\x81\xc2\x81", 4, &output));
  EXPECT_EQ(output.compare("\xc2\x81\xc2\x81"), 0);

  {
    const char array[] = "\x00\x00\xc2\x81\xc2\x81";
    const std::string array_string(array, size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\x00\x00\xc2\x81", 4)), 0);
  }

  {
    const char array[] = "\x00\xc2\x81\xc2\x81";
    const std::string array_string(array, size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\x00\xc2\x81", 3)), 0);
  }

  // Testing invalid UTF8
  EXPECT_TRUE(Truncated("\xed\xa0\x80\xed\xbf\xbf", 6, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xed\xa0\x8f", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xed\xbf\xbf", 3, &output));
  EXPECT_EQ(output.compare(""), 0);

  // Testing invalid UTF8 mixed with valid UTF8
  EXPECT_FALSE(Truncated("\xe1\x80\xbf", 3, &output));
  EXPECT_EQ(output.compare("\xe1\x80\xbf"), 0);
  EXPECT_FALSE(Truncated("\xf1\x80\xa0\xbf", 4, &output));
  EXPECT_EQ(output.compare("\xf1\x80\xa0\xbf"), 0);
  EXPECT_FALSE(Truncated("a\xc2\x81\xe1\x80\xbf\xf1\x80\xa0\xbf", 10, &output));
  EXPECT_EQ(output.compare("a\xc2\x81\xe1\x80\xbf\xf1\x80\xa0\xbf"), 0);
  EXPECT_TRUE(
      Truncated("a\xc2\x81\xe1\x80\xbf\xf1"
                "a"
                "\x80\xa0",
                10, &output));
  EXPECT_EQ(output.compare("a\xc2\x81\xe1\x80\xbf\xf1"
                           "a"),
            0);
  EXPECT_FALSE(
      Truncated("\xef\xbb\xbf"
                "abc",
                6, &output));
  EXPECT_EQ(output.compare("\xef\xbb\xbf"
                           "abc"),
            0);

  // Overlong sequences
  EXPECT_TRUE(Truncated("\xc0\x80", 2, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xc1\x80\xc1\x81", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xe0\x80\x80", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xe0\x82\x80", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xe0\x9f\xbf", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x80\x80\x8D", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x80\x82\x91", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x80\xa0\x80", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x8f\xbb\xbf", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf8\x80\x80\x80\xbf", 5, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xfc\x80\x80\x80\xa0\xa5", 6, &output));
  EXPECT_EQ(output.compare(""), 0);

  // Beyond U+10FFFF (the upper limit of Unicode codespace)
  EXPECT_TRUE(Truncated("\xf4\x90\x80\x80", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf8\xa0\xbf\x80\xbf", 5, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xfc\x9c\xbf\x80\xbf\x80", 6, &output));
  EXPECT_EQ(output.compare(""), 0);

  // BOMs in UTF-16(BE|LE) and UTF-32(BE|LE)
  EXPECT_TRUE(Truncated("\xfe\xff", 2, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xff\xfe", 2, &output));
  EXPECT_EQ(output.compare(""), 0);

  {
    const char array[] = "\x00\x00\xfe\xff";
    const std::string array_string(array, size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\x00\x00", 2)), 0);
  }

  // Variants on the previous test
  {
    const char array[] = "\xff\xfe\x00\x00";
    const std::string array_string(array, 4);
    EXPECT_FALSE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\xff\xfe\x00\x00", 4)), 0);
  }
  {
    const char array[] = "\xff\x00\x00\xfe";
    const std::string array_string(array, size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\xff\x00\x00", 3)), 0);
  }

  // Non-characters : U+xxFFF[EF] where xx is 0x00 through 0x10 and <FDD0,FDEF>
  EXPECT_TRUE(Truncated("\xef\xbf\xbe", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x8f\xbf\xbe", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf3\xbf\xbf\xbf", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xef\xb7\x90", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xef\xb7\xaf", 3, &output));
  EXPECT_EQ(output.compare(""), 0);

  // Strings in legacy encodings that are valid in UTF-8, but
  // are invalid as UTF-8 in real data.
  EXPECT_TRUE(Truncated("caf\xe9", 4, &output));
  EXPECT_EQ(output.compare("caf"), 0);
  EXPECT_TRUE(Truncated("\xb0\xa1\xb0\xa2", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_FALSE(Truncated("\xa7\x41\xa6\x6e", 4, &output));
  EXPECT_EQ(output.compare("\xa7\x41\xa6\x6e"), 0);
  EXPECT_TRUE(Truncated("\xa7\x41\xa6\x6e\xd9\xee\xe4\xee", 7, &output));
  EXPECT_EQ(output.compare("\xa7\x41\xa6\x6e"), 0);

  // Testing using the same string as input and output.
  EXPECT_FALSE(Truncated(output, 4, &output));
  EXPECT_EQ(output.compare("\xa7\x41\xa6\x6e"), 0);
  EXPECT_TRUE(Truncated(output, 3, &output));
  EXPECT_EQ(output.compare("\xa7\x41"), 0);

  // "abc" with U+201[CD] in windows-125[0-8]
  EXPECT_TRUE(
      Truncated("\x93"
                "abc\x94",
                5, &output));
  EXPECT_EQ(output.compare("\x93"
                           "abc"),
            0);

  // U+0639 U+064E U+0644 U+064E in ISO-8859-6
  EXPECT_TRUE(Truncated("\xd9\xee\xe4\xee", 4, &output));
  EXPECT_EQ(output.compare(""), 0);

  // U+03B3 U+03B5 U+03B9 U+03AC in ISO-8859-7
  EXPECT_TRUE(Truncated("\xe3\xe5\xe9\xdC", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
}

}  // namespace utils
}  // namespace nearby
