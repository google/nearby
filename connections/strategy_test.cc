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

#include "connections/strategy.h"

#include "gtest/gtest.h"

namespace nearby {
namespace connections {

TEST(StrategyTest, IsValidWorks) {
  EXPECT_FALSE(Strategy().IsValid());
  EXPECT_TRUE(Strategy::kP2pCluster.IsValid());
  EXPECT_TRUE(Strategy::kP2pStar.IsValid());
  EXPECT_TRUE(Strategy::kP2pPointToPoint.IsValid());
}

TEST(StrategyTest, IsNoneWorks) {
  EXPECT_TRUE(Strategy().IsNone());
  EXPECT_FALSE(Strategy::kP2pCluster.IsNone());
  EXPECT_FALSE(Strategy::kP2pStar.IsNone());
  EXPECT_FALSE(Strategy::kP2pPointToPoint.IsNone());
}

TEST(StrategyTest, CompareWorks) {
  EXPECT_EQ(Strategy::kP2pCluster, Strategy::kP2pCluster);
  EXPECT_EQ(Strategy::kP2pStar, Strategy::kP2pStar);
  EXPECT_EQ(Strategy::kP2pPointToPoint, Strategy::kP2pPointToPoint);
  EXPECT_NE(Strategy::kP2pCluster, Strategy::kP2pStar);
  EXPECT_NE(Strategy::kP2pCluster, Strategy::kP2pPointToPoint);
  EXPECT_NE(Strategy::kP2pStar, Strategy::kP2pPointToPoint);
}

TEST(StrategyTest, GetNameWorks) {
  EXPECT_EQ(Strategy().GetName(), "UNKNOWN");
  EXPECT_EQ(Strategy::kP2pCluster.GetName(), "P2P_CLUSTER");
  EXPECT_EQ(Strategy::kP2pStar.GetName(), "P2P_STAR");
  EXPECT_EQ(Strategy::kP2pPointToPoint.GetName(), "P2P_POINT_TO_POINT");
}

}  // namespace connections
}  // namespace nearby
