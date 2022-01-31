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

#include "internal/platform/logging.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace {

TEST(LoggingTest, CanLog) {
  NEARBY_LOG_SET_SEVERITY(INFO);
  int num = 42;
  NEARBY_LOG(INFO, "The answer to everything: %d", num++);
  EXPECT_EQ(num, 43);
}

TEST(LoggingTest, CanLog_LoggingDisabled) {
  NEARBY_LOG_SET_SEVERITY(ERROR);
  int num = 42;
  NEARBY_LOG(INFO, "The answer to everything: %d", num++);
  // num++ should not be evaluated
  EXPECT_EQ(num, 42);
}

TEST(LoggingTest, CanStream) {
  NEARBY_LOG_SET_SEVERITY(INFO);
  int num = 42;
  NEARBY_LOGS(INFO) << "The answer to everything: " << num++;
  EXPECT_EQ(num, 43);
}

TEST(LoggingTest, CanStream_LoggingDisabled) {
  NEARBY_LOG_SET_SEVERITY(ERROR);
  int num = 42;
  NEARBY_LOGS(INFO) << "The answer to everything: " << num++;
  // num++ should not be evaluated
  EXPECT_EQ(num, 42);
}

}  // namespace
