#include "platform/public/count_down_latch.h"

#include "platform/public/single_thread_executor.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

TEST(CountDownLatch, ConstructorDestructorWorks) { CountDownLatch latch(1); }

TEST(CountDownLatch, LatchAwaitCanWait) {
  CountDownLatch latch(1);
  SingleThreadExecutor executor;
  std::atomic_bool done = false;
  executor.Execute([&done, &latch]() {
    done = true;
    latch.CountDown();
  });
  latch.Await();
  EXPECT_TRUE(done);
}

TEST(CountDownLatch, LatchExtraCountDownIgnored) {
  CountDownLatch latch(1);
  SingleThreadExecutor executor;
  std::atomic_bool done = false;
  executor.Execute([&done, &latch]() {
    done = true;
    latch.CountDown();
    latch.CountDown();
    latch.CountDown();
  });
  latch.Await();
  EXPECT_TRUE(done);
}

TEST(CountDownLatch, LatchAwaitWithTimeoutCanExpire) {
  CountDownLatch latch(1);
  SingleThreadExecutor executor;
  auto response = latch.Await(absl::Milliseconds(100));
  EXPECT_TRUE(response.ok());
  EXPECT_FALSE(response.result());
}

}  // namespace
}  // namespace nearby
}  // namespace location
