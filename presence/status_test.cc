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

#include "presence/status.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace presence {
namespace {

TEST(StatusTest, DefaultIsError) {
  Status status;
  EXPECT_FALSE(status.Ok());
  EXPECT_EQ(status, Status{Status::Value::kError});
}

TEST(StatusTest, DefaultEquals) {
  Status status1;
  Status status2;
  EXPECT_EQ(status1, status2);
}

TEST(StatusTest, ExplicitInitEquals) {
  Status status1 = {Status::Value::kSuccess};
  Status status2 = {Status::Value::kSuccess};
  EXPECT_EQ(status1, status2);
  EXPECT_TRUE(status1.Ok());
}

TEST(StatusTest, ExplicitInitNotEquals) {
  Status status1 = {Status::Value::kSuccess};
  Status status2 = {Status::Value::kError};
  EXPECT_NE(status1, status2);
}

TEST(StatusTest, CopyInitEquals) {
  Status status1 = {Status::Value::kSuccess};
  Status status2 = {status1};
  EXPECT_EQ(status1, status2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
