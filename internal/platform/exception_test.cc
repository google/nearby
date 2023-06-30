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

#include "internal/platform/exception.h"

#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/exception_test.nc.h"

namespace nearby {

TEST(ExceptionOr, Result_Copy_NonConst) {
  ExceptionOr<std::vector<int>> exception_or_vector({1, 2, 3});
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Expect a copy when not explicitly moving the result.
  std::vector<int> copy = exception_or_vector.result();
  EXPECT_FALSE(copy.empty());
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Modifying |exception_or_vector| should not affect the copy.
  exception_or_vector.result().clear();
  EXPECT_FALSE(copy.empty());
}

TEST(ExceptionOr, Result_Copy_Const) {
  const ExceptionOr<std::vector<int>> exception_or_vector({1, 2, 3});
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Expect a copy when not explicitly moving the result.
  const std::vector<int>& copy = exception_or_vector.result();
  EXPECT_FALSE(copy.empty());
  EXPECT_FALSE(exception_or_vector.result().empty());
}

TEST(ExceptionOr, Result_Reference_NonConst) {
  ExceptionOr<std::vector<int>> exception_or_vector({1, 2, 3});
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Getting a reference should not modify the source.
  std::vector<int>& reference = exception_or_vector.result();
  EXPECT_FALSE(reference.empty());
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Modifying |exception_or_vector| should reflect in the reference.
  exception_or_vector.result().clear();
  EXPECT_TRUE(reference.empty());
}

TEST(ExceptionOr, Result_Reference_Const) {
  const ExceptionOr<std::vector<int>> exception_or_vector({1, 2, 3});
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Getting a reference should not modify the source.
  const std::vector<int>& reference = exception_or_vector.result();
  EXPECT_FALSE(reference.empty());
  EXPECT_FALSE(exception_or_vector.result().empty());
}

TEST(ExceptionOr, Result_Move_NonConst) {
  ExceptionOr<std::vector<int>> exception_or_vector({1, 2, 3});
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Moving the result should clear the source.
  std::vector<int> moved = std::move(exception_or_vector).result();
  EXPECT_FALSE(moved.empty());
}

TEST(ExceptionOr, Result_Move_Const) {
  const ExceptionOr<std::vector<int>> exception_or_vector({1, 2, 3});
  EXPECT_FALSE(exception_or_vector.result().empty());

  // Moving const rvalue reference will result in a copy.
  std::vector<int> moved = std::move(exception_or_vector).result();
  EXPECT_FALSE(moved.empty());
}

TEST(ExceptionOr, ExplicitConversionWorks) {
  class A {
   public:
    A() = default;
  };
  class B {
   public:
    B() = default;
    explicit B(A) {}
  };
  ExceptionOr<A> a(A{});
  ExceptionOr<B> b(a);
  EXPECT_TRUE(a.ok());
  EXPECT_TRUE(b.ok());
}

TEST(ExceptionOr, ExplicitConversionFailsToCompile) {
  class A {
   public:
    A() = default;
  };
  class B {
   public:
    B() = default;
  };
  ExceptionOr<A> a(A{});
  EXPECT_NON_COMPILE("no matching constructor", { ExceptionOr<B> b(a); });
}

TEST(ExceptionOr, BoolType) {
  EXPECT_TRUE(ExceptionOr<bool>(true));
  EXPECT_TRUE(ExceptionOr<bool>(true).GetResult());
  EXPECT_TRUE(ExceptionOr<bool>(true).result());
  EXPECT_TRUE(ExceptionOr<bool>(true).ok());
  EXPECT_EQ(ExceptionOr<bool>(true).exception(), Exception::kSuccess);
  EXPECT_TRUE(ExceptionOr<bool>(Exception::kSuccess));
  EXPECT_FALSE(ExceptionOr<bool>(false));
  EXPECT_FALSE(ExceptionOr<bool>(false).GetResult());
  EXPECT_FALSE(ExceptionOr<bool>(false).result());
  EXPECT_FALSE(ExceptionOr<bool>(false).ok());
  EXPECT_EQ(ExceptionOr<bool>(false).exception(), Exception::kFailed);
  EXPECT_FALSE(ExceptionOr<bool>());
  EXPECT_FALSE(ExceptionOr<bool>(Exception::kInterrupted));
  EXPECT_EQ(ExceptionOr<bool>(Exception::kInterrupted).exception(),
            Exception::kInterrupted);
}

}  // namespace nearby
