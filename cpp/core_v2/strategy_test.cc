#include "core_v2/strategy.h"

#include "gtest/gtest.h"

namespace location {
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
}  // namespace location
