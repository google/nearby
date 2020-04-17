// Copyright 2020 Google LLC
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

#include "platform/container_of.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location::nearby {

TEST(OffsetOf, OffsetOfTest) {
  struct [[gnu::packed]] S {
    char x;
    double y;
  };
  EXPECT_EQ(OffsetOf(&S::x), 0U);
  EXPECT_EQ(OffsetOf(&S::y), sizeof(S::x));
}

TEST(OffsetOf, ExplicitOffsetOfTest) {
  struct [[gnu::packed]] S1 { int x; };
  struct [[gnu::packed]] S2 { double y; };
  struct [[gnu::packed]] S : public S1, S2 { char t; };
  EXPECT_EQ((OffsetOf<decltype(std::declval<S>().x), S>(&S::x)), 0U);
  EXPECT_EQ((OffsetOf<decltype(std::declval<S>().y), S>(&S::y)), sizeof(S::x));
}

TEST(ContainerOf, ContainerOfTest) {
  struct [[gnu::packed]] S {
    char x;
    double y;
  } s;
  char* p = &s.x;
  double* q = &s.y;
  EXPECT_EQ(ContainerOf(p, &S::x), &s);
  EXPECT_EQ(ContainerOf(q, &S::y), &s);
}

TEST(ContainerOf, ContainerOfTestConst) {
  struct [[gnu::packed]] S {
    char x;
    double y;
  } s;
  const char* p = &s.x;
  const double* q = &s.y;
  EXPECT_EQ(ContainerOf(p, &S::x), &s);
  EXPECT_EQ(ContainerOf(q, &S::y), &s);
}

}  // namespace location::nearby
