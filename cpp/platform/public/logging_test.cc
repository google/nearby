#include "platform/public/logging.h"

#include "gmock/gmock.h"
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
