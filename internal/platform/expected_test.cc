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

#include "internal/platform/expected.h"

#include <memory>

#include "gtest/gtest.h"
#include "proto/connections_enums.proto.h"

namespace nearby {
using ::location::nearby::proto::connections::OperationResultCode;

TEST(ExpectedTest, Expected) {
  ErrorOr<int> value = 42;
  EXPECT_TRUE(value.has_value());
  EXPECT_FALSE(value.has_error());
  EXPECT_EQ(*value, 42);

  ErrorOr<int> error{Error(OperationResultCode::DETAIL_UNKNOWN)};
  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(error.has_error());
  EXPECT_EQ(error.error().operation_result_code(),
            OperationResultCode::DETAIL_UNKNOWN);

  ErrorOr<std::unique_ptr<int>> value2{std::make_unique<int>(42)};
  EXPECT_TRUE(value2.has_value());
  EXPECT_FALSE(value2.has_error());
  EXPECT_NE(value2.value().get(), nullptr);
  EXPECT_EQ(*value2.value(), 42);

  ErrorOr<std::unique_ptr<int>> error2{
      Error(OperationResultCode::IO_FILE_OPENING_ERROR)};
  EXPECT_FALSE(error2.has_value());
  EXPECT_TRUE(error2.has_error());
  EXPECT_EQ(error2.error().operation_result_code(),
            OperationResultCode::IO_FILE_OPENING_ERROR);
}

}  // namespace nearby
