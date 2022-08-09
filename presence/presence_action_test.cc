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

#include "presence/presence_action.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace presence {
namespace {
constexpr int kDefaultActionIdentifier = 1;
constexpr int kTestActionIdentifier = 2;
TEST(PresenceActionTest, DefaultConstructorWorks) {
  PresenceAction action;
  EXPECT_EQ(action.GetActionIdentifier(), kDefaultActionIdentifier);
}

TEST(PresenceActionTest, DefaultEquals) {
  PresenceAction action1;
  PresenceAction action2;
  EXPECT_EQ(action1, action2);
}

TEST(PresenceActionTest, ExplicitInitEquals) {
  PresenceAction action1 = {kTestActionIdentifier};
  PresenceAction action2 = {kTestActionIdentifier};
  EXPECT_EQ(action1, action2);
  EXPECT_EQ(action1.GetActionIdentifier(), kTestActionIdentifier);
}

TEST(PresenceActionTest, ExplicitInitNotEquals) {
  PresenceAction action1 = {kDefaultActionIdentifier};
  PresenceAction action2 = {kTestActionIdentifier};
  EXPECT_NE(action1, action2);
}

TEST(PresenceActionTest, CopyInitEquals) {
  PresenceAction action1 = {kTestActionIdentifier};
  PresenceAction action2 = {action1};

  EXPECT_EQ(action1, action2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
