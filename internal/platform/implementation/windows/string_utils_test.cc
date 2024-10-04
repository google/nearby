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
#include "internal/platform/implementation/windows/string_utils.h"

#include <sstream>
#include <string>

#include "gtest/gtest.h"

namespace nearby::windows::string_utils {

const wchar_t* const kConvertRoundtripCasesWide[] = {
    L"Hello World!",
    L"Quick Share for Windows",
    // "附近分享 蓝牙 无线传送 »"
    L"\x9644\x8fd1\x5206\x4eab\x0020\x84dd\x7259\x0020\x65e0\x7ebf\x4f20\x9001"
    L"\x0020\x00bb",
    // "近隣のシェア"
    L"\x8fd1\x96a3\x306e\x30b7\x30a7\x30a2",
    // "주변 공유"
    L"\xc8fc\xbcc0\x0020\xacf5\xc720",
    // "आस-पास साझा करें"
    L"\x0906\x0938\x002d\x092a\x093e\x0938\x0020\x0938\x093e\x091d\x093e\x0020"
    L"\x0915\x0930\x0947\x0902",
    // "அருகிலுள்ள பகிர்வு"
    L"\x0b85\x0bb0\x0bc1\x0b95\x0bbf\x0bb2\x0bc1\x0bb3\x0bcd\x0bb3\x0020\x0baa"
    L"\x0b95\x0bbf\x0bb0\x0bcd\x0bb5\x0bc1",
};

const wchar_t* const kWideStringWithNull[] = {
    L"A" L"\0" L"B" L"\0" L"C",
    L"Hello World!"
    L"\0"
    L"Quick Share for Windows"
    L"\0"
    // "附近分享 蓝牙 无线传送 »"
    L"\x9644\x8fd1\x5206\x4eab\x0020\x84dd\x7259\x0020\x65e0\x7ebf\x4f20\x9001"
    L"\x0020\x00bb",
};

const wchar_t* const kSymbolsWideString[] = {
    // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 :
    // A,B,C,D,E)
    L"\xd807\xdd40\xd807\xdd41\xd807\xdd42\xd807\xdd43\xd807\xdd44",
};

const char* const kConvertRoundtripCases[] = {
    "Hello World!",
    "Quick Share for Windows",
    "附近分享 蓝牙 无线传送 »",
    "近隣のシェア",
    "주변 공유",
    "आस-पास साझा करें",
    "அருகிலுள்ள பகிர்வு",
};

const char* const kStringWithNull[] = {
    "A" "\0" "B" "\0" "C",
    "Hello World!"
    "\0"
    "Quick Share for Windows"
    "\0"
    "附近分享 蓝牙 无线传送 »",
};

const char* const kStringWithoutNull[] = {
    "ABC",
    "Hello World!"
    "Quick Share for Windows"
    "附近分享 蓝牙 无线传送 »",
};

const char* const kIllegalString[] = {
    "©pj·Å",  // base64 decoded string of "malware.exe"
};

TEST(StringUtilsTests, ConvertWideStringToString) {
  int count = sizeof(kConvertRoundtripCasesWide) /
              sizeof(kConvertRoundtripCasesWide[0]);
  for (int i = 0; i < count; ++i) {
    std::ostringstream utf8;
    utf8 << WideStringToString(kConvertRoundtripCasesWide[i]);
    EXPECT_EQ(utf8.str(), kConvertRoundtripCases[i]);
  }

  count = sizeof(kWideStringWithNull) /
              sizeof(kWideStringWithNull[0]);
  for (int i = 0; i < count; ++i) {
    std::ostringstream utf8;
    utf8 << WideStringToString(kWideStringWithNull[i]);
    EXPECT_EQ(utf8.str(), kStringWithNull[i]);
    EXPECT_NE(utf8.str(), kStringWithoutNull[i]);
  }
}

TEST(StringUtilsTests, ConvertWideStringToStringRoundTrip) {
  // we round-trip all the wide strings through UTF-8 to make sure everything
  // agrees on the conversion. This uses the stream operators to test them
  // simultaneously.
  for (auto* i : kConvertRoundtripCasesWide) {
    std::ostringstream utf8;
    utf8 << WideStringToString(i);
    std::wostringstream wide;
    wide << StringToWideString(utf8.str());

    EXPECT_EQ(i, wide.str());
  }

  for (auto* i : kSymbolsWideString) {
    std::ostringstream utf8;
    utf8 << WideStringToString(i);
    std::wostringstream wide;
    wide << StringToWideString(utf8.str());

    EXPECT_EQ(i, wide.str());
  }

  for (auto* i : kWideStringWithNull) {
    std::ostringstream utf8;
    utf8 << WideStringToString(i);
    std::wostringstream wide;
    wide << StringToWideString(utf8.str());

    EXPECT_EQ(i, wide.str());
  }
}

TEST(StringUtilsTests, ConvertStringToWideString) {
  int count = sizeof(kConvertRoundtripCasesWide) /
              sizeof(kConvertRoundtripCasesWide[0]);
  for (int i = 0; i < count; ++i) {
    std::wostringstream wide;
    wide << StringToWideString(kConvertRoundtripCases[i]);
    EXPECT_EQ(wide.str(), kConvertRoundtripCasesWide[i]);
  }
}

TEST(StringUtilsTests, ConvertStringToWideStringRoundTrip) {
  // we round-trip all the wide strings through UTF-8 to make sure everything
  // agrees on the conversion. This uses the stream operators to test them
  // simultaneously.
  for (auto* i : kConvertRoundtripCases) {
    std::wostringstream wide;
    wide << StringToWideString(i);
    std::ostringstream utf8;
    utf8 << WideStringToString(wide.str());

    EXPECT_EQ(i, utf8.str());
  }

  for (auto* i : kIllegalString) {
    std::wostringstream wide;
    wide << StringToWideString(i);
    std::ostringstream utf8;
    utf8 << WideStringToString(wide.str());

    EXPECT_EQ(i, utf8.str());
  }

  for (auto* i : kStringWithNull) {
    std::wostringstream wide;
    wide << StringToWideString(i);
    std::ostringstream utf8;
    utf8 << WideStringToString(wide.str());

    EXPECT_EQ(i, utf8.str());
  }
}

TEST(StringUtilsTests, ConvertEmptyStringAndWideString) {
  // An empty std::wstring should be converted to an empty std::string,
  // and vice versa.
  std::wstring wide_empty;
  std::string empty;
  EXPECT_EQ(empty, WideStringToString(wide_empty));
  EXPECT_EQ(wide_empty, StringToWideString(empty));
}

}  // namespace nearby::windows::string_utils
