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

#include "gtest/gtest.h"

namespace nearby {
namespace masker {
namespace {

TEST(MaskerTest, DefaultMaskEmptyString) { EXPECT_EQ(Mask(""), ""); }

TEST(MaskerTest, DefaultMaskShortString) {
  EXPECT_EQ(Mask("a"), "a");
  EXPECT_EQ(Mask("ab"), "ab");
}

TEST(MaskerTest, DefaultMaskLongerString) {
  EXPECT_EQ(Mask("abc"), "ab*");
  EXPECT_EQ(Mask("abcd"), "ab**");
  EXPECT_EQ(Mask("abcdef"), "ab****");
}

TEST(MaskerTest, CustomMaskEmptyString) {
  EXPECT_EQ(Mask("", '#', 0), "");
  EXPECT_EQ(Mask("", '#', 5), "");
}

TEST(MaskerTest, CustomMaskStartIndexZero) {
  EXPECT_EQ(Mask("abc", '#', 0), "###");
  EXPECT_EQ(Mask("hello", 'X', 0), "XXXXX");
}

TEST(MaskerTest, CustomMaskStartIndexOne) {
  EXPECT_EQ(Mask("abc", '#', 1), "a##");
  EXPECT_EQ(Mask("hello", 'X', 1), "hXXXX");
}

TEST(MaskerTest, CustomMaskStartIndexGreaterThanLength) {
  EXPECT_EQ(Mask("abc", '#', 3), "abc");
  EXPECT_EQ(Mask("abc", '#', 5), "abc");
}

TEST(MaskerTest, CustomMaskStartIndexNegative) {
  EXPECT_EQ(Mask("abc", '#', -1), "###");
  EXPECT_EQ(Mask("hello", 'X', -10), "XXXXX");
}

TEST(MaskerTest, CustomMaskDifferentChar) {
  EXPECT_EQ(Mask("password", '$', 4), "pass$$$$");
  EXPECT_EQ(Mask("secret", '?', 2), "se????");
}

}  // namespace
}  // namespace masker
}  // namespace nearby
