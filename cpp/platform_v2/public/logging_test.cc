#include "platform_v2/public/logging.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(LoggingTest, CanLog) {
  NEARBY_LOG(INFO, "message");
}

}
