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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::internal::IdentityType;

constexpr IdentityType kTestIdentityType =
    IdentityType::IDENTITY_TYPE_CONTACTS_GROUP;

TEST(PresenceIdentityTest, ExplicitInitEquals) {
  IdentityType identity1 = {kTestIdentityType};
  IdentityType identity2 = {kTestIdentityType};
  EXPECT_EQ(identity1, identity2);
  EXPECT_EQ(identity1, kTestIdentityType);
}


TEST(PresenceIdentityTest, CopyInitEquals) {
  IdentityType identity1 = {kTestIdentityType};
  IdentityType identity2 = {identity1};
  EXPECT_EQ(identity1, identity2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
