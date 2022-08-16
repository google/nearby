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

#include "presence/discovery_filter.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace presence {
namespace {

using ::nearby::internal::IdentityType;

const PresenceAction kTestAction = {1};
const IdentityType kTestIdentity = {IdentityType::IDENTITY_TYPE_TRUSTED};

TEST(DiscoveryFilterTest, DefaultConstructorWorks) {
  DiscoveryFilter filter;
  EXPECT_EQ(filter.GetActions().size(), 0);
  EXPECT_EQ(filter.GetIdentities().size(), 0);
  EXPECT_EQ(filter.GetZones().size(), 0);
}

TEST(DiscoveryFilterTest, PartiallyInitializationWorks) {
  DiscoveryFilter filter1{{kTestAction}, {kTestIdentity}};
  DiscoveryFilter filter2{{kTestAction}};
  EXPECT_EQ(filter1.GetActions(), filter2.GetActions());
  EXPECT_NE(filter1.GetIdentities(), filter2.GetIdentities());
  EXPECT_EQ(filter1.GetZones(), filter2.GetZones());

  EXPECT_EQ(filter1.GetActions()[0], kTestAction);
  EXPECT_EQ(filter1.GetIdentities()[0], kTestIdentity);
  EXPECT_EQ(filter1.GetZones().capacity(), 0);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
